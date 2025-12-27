#include "backend/PerformanceMonitor.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMetaObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QProcess>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <utility>

#include <unistd.h>

namespace {
constexpr int kHistorySizeLimit = 120;
constexpr int kHistorySignalTailLimit = 120;
constexpr int kResourceSampleIntervalMs = 1000;
constexpr qint64 kFrameTimeoutNs = static_cast<qint64>(5) * 1000 * 1000 * 1000; // 5 seconds

const QString kRknpuDebugRoot = QStringLiteral("/sys/kernel/debug/rknpu");

inline qint64 toNanoseconds(const std::chrono::steady_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
}

inline qint64 toMicroseconds(const std::chrono::steady_clock::duration& duration) {
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

inline quint64 bytesToKilobytes(quint64 bytes) {
    return bytes / 1024;
}

QString formatFrequencyFromHz(const QString& raw) {
    bool ok = false;
    const quint64 hz = raw.toULongLong(&ok);
    if (!ok || hz == 0) {
        return raw;
    }
    const double mhz = static_cast<double>(hz) / 1'000'000.0;
    return QStringLiteral("%1 MHz (%2 Hz)")
        .arg(QString::number(mhz, 'f', mhz >= 100.0 ? 0 : 2), QString::number(hz));
}

QString formatVoltageFromMicroVolts(const QString& raw) {
    bool ok = false;
    const quint64 microVolts = raw.toULongLong(&ok);
    if (!ok || microVolts == 0) {
        return raw;
    }
    const double volts = static_cast<double>(microVolts) / 1'000'000.0;
    const double milliVolts = static_cast<double>(microVolts) / 1'000.0;
    return QStringLiteral("%1 V (%2 mV)")
        .arg(QString::number(volts, 'f', 3), QString::number(milliVolts, 'f', milliVolts >= 100.0 ? 0 : 1));
}

QString formatDelayMs(const QString& raw) {
    bool ok = false;
    const quint64 milliseconds = raw.toULongLong(&ok);
    if (!ok) {
        return raw;
    }
    return QStringLiteral("%1 ms").arg(milliseconds);
}

bool tryParsePercentage(const QByteArray& content, double* valueOut) {
    if (!valueOut) {
        return false;
    }

    QRegularExpression re(QStringLiteral(R"((\d+(?:\.\d+)?)\s*%?)"));
    QRegularExpressionMatch match = re.match(QString::fromUtf8(content));
    if (!match.hasMatch()) {
        return false;
    }

    bool ok = false;
    double parsed = match.captured(1).toDouble(&ok);
    if (!ok) {
        return false;
    }
    *valueOut = parsed;
    return true;
}

} // namespace

PerformanceMonitor* PerformanceMonitor::instance() {
    static PerformanceMonitor* s_instance = new PerformanceMonitor();
    return s_instance;
}

PerformanceMonitor::PerformanceMonitor(QObject* parent)
    : QObject(parent)
    , m_resourceTimer(this)
    , m_nextFrameId(1)
    , m_npuContext(0)
    , m_lastProcessTicks(0)
    , m_lastTotalTicks(0)
    , m_lastActiveTicks(0)
    , m_pendingProfilingInferences(0) {

    connect(&m_resourceTimer, &QTimer::timeout, this, &PerformanceMonitor::sampleResourceUsage);
    m_resourceTimer.setInterval(kResourceSampleIntervalMs);
    m_resourceTimer.start();

    // Initialize resource snapshot immediately so UI has baseline values.
    sampleResourceUsage();
}

quint64 PerformanceMonitor::recordFrameCaptured(qint64 captureTimestampNs) {
    if (captureTimestampNs <= 0) {
        captureTimestampNs = steadyNowNs();
    }

    QMutexLocker locker(&m_mutex);
    const quint64 frameId = m_nextFrameId++;
    FrameRecord record;
    record.captureTimestampNs = captureTimestampNs;
    m_pendingFrames.emplace(frameId, record);

    pruneStaleFrames(captureTimestampNs);
    return frameId;
}

void PerformanceMonitor::recordDetectionStages(quint64 frameId,
                                               qint64 captureToPreprocessUs,
                                               qint64 preprocessUs,
                                               qint64 npuUs,
                                               qint64 postprocessUs,
                                               qint64 detectionCompleteTimestampNs) {
    if (frameId == 0) {
        return;
    }

    if (detectionCompleteTimestampNs <= 0) {
        detectionCompleteTimestampNs = steadyNowNs();
    }

    QMutexLocker locker(&m_mutex);
    auto it = m_pendingFrames.find(frameId);
    if (it == m_pendingFrames.end()) {
        FrameRecord record;
        record.captureTimestampNs = detectionCompleteTimestampNs;
        it = m_pendingFrames.emplace(frameId, record).first;
    }

    FrameRecord& record = it->second;
    record.captureToPreprocessUs = captureToPreprocessUs;
    record.preprocessUs = preprocessUs;
    record.npuUs = npuUs;
    record.postprocessUs = postprocessUs;
    record.detectionCompleteTimestampNs = detectionCompleteTimestampNs;

    pruneStaleFrames(detectionCompleteTimestampNs);
}

void PerformanceMonitor::recordRenderStage(quint64 frameId,
                                           qint64 renderDurationUs,
                                           qint64 renderCompleteTimestampNs) {
    if (frameId == 0) {
        return;
    }

    if (renderCompleteTimestampNs <= 0) {
        renderCompleteTimestampNs = steadyNowNs();
    }

    FrameTimings summary;
    QVector<FrameTimings> historyCopy;

    {
        QMutexLocker locker(&m_mutex);
        auto it = m_pendingFrames.find(frameId);
        if (it == m_pendingFrames.end()) {
            return;
        }

        FrameRecord& record = it->second;
        record.renderUs = renderDurationUs;
        record.renderCompleteTimestampNs = renderCompleteTimestampNs;

        summary.frameId = frameId;
        summary.captureTimestampNs = record.captureTimestampNs;
        summary.captureToPreprocessUs = record.captureToPreprocessUs;
        summary.preprocessUs = record.preprocessUs;
        summary.npuUs = record.npuUs;
        summary.postprocessUs = record.postprocessUs;
        summary.renderUs = record.renderUs;
        if (record.captureTimestampNs > 0) {
            summary.totalLatencyUs = (record.renderCompleteTimestampNs - record.captureTimestampNs) / 1000;
        }

        m_history.push_back(summary);
        if (m_history.size() > kHistorySizeLimit) {
            m_history.pop_front();
        }

        m_pendingFrames.erase(it);

        const int available = static_cast<int>(m_history.size());
        const int copyCount = std::min(available, kHistorySignalTailLimit);
        historyCopy.reserve(copyCount);
        const int startIndex = available - copyCount;
        for (int i = startIndex; i < available; ++i) {
            historyCopy.append(m_history[static_cast<std::size_t>(i)]);
        }
    }

    emit frameMetricsUpdated(summary, historyCopy);
}

QVector<PerformanceMonitor::FrameTimings> PerformanceMonitor::recentFrameHistory(int maxSamples) const {
    QMutexLocker locker(&m_mutex);
    QVector<FrameTimings> historyCopy;
    if (m_history.empty()) {
        return historyCopy;
    }

    const int available = static_cast<int>(m_history.size());
    const int count = maxSamples > 0 ? std::min(maxSamples, available) : available;
    historyCopy.reserve(count);

    const int startIndex = available - count;
    for (int i = startIndex; i < available; ++i) {
        historyCopy.append(m_history[static_cast<std::size_t>(i)]);
    }
    return historyCopy;
}

PerformanceMonitor::ResourceSnapshot PerformanceMonitor::latestResourceSnapshot() const {
    QMutexLocker locker(&m_mutex);
    return m_latestResources;
}

void PerformanceMonitor::setNpuContext(rknn_context context) {
    {
        QMutexLocker locker(&m_npuMutex);
        m_npuContext = context;
    }
    QMetaObject::invokeMethod(this,
                              &PerformanceMonitor::sampleResourceUsage,
                              Qt::QueuedConnection);
}

void PerformanceMonitor::requestProfiling() {
    emit profilingRequested();
}

void PerformanceMonitor::submitNpuPerfDetail(const QString& report) {
    {
        QMutexLocker locker(&m_npuMutex);
        if (m_pendingProfilingInferences > 0) {
            m_npuPerfReports.append(report);
            m_pendingProfilingInferences--;
            
            // Request profiling for next inference if needed
            if (m_pendingProfilingInferences > 0) {
                locker.unlock();
                requestProfiling();
                emit npuPerfDetailUpdated(report);
                return;
            }
        }
    }
    
    // Always emit the updated report
    emit npuPerfDetailUpdated(report);
}

void PerformanceMonitor::ensureNpuStaticInfo() {
    QMutexLocker locker(&m_npuInfoMutex);

    auto readTrimmed = [](const QString& path, QString* out, QStringList* errors) {
        QByteArray content;
        QString errorDetail;
        if (!PerformanceMonitor::readFileContent(path, &content, &errorDetail)) {
            if (!errorDetail.isEmpty()) {
                errors->append(errorDetail);
            }
            return;
        }

        const QString trimmed = QString::fromUtf8(content).trimmed();
        if (trimmed.isEmpty()) {
            errors->append(QObject::tr("%1 返回空内容").arg(path));
            return;
        }
        *out = trimmed;
    };

    QStringList errors;

    if (m_npuStaticInfo.frequency.isEmpty()) {
        QString value;
        readTrimmed(kRknpuDebugRoot + QStringLiteral("/freq"), &value, &errors);
        if (!value.isEmpty()) {
            m_npuStaticInfo.frequency = formatFrequencyFromHz(value);
        }
    }

    if (m_npuStaticInfo.powerState.isEmpty()) {
        QString value;
        readTrimmed(kRknpuDebugRoot + QStringLiteral("/power"), &value, &errors);
        if (!value.isEmpty()) {
            m_npuStaticInfo.powerState = value;
        }
    }

    if (m_npuStaticInfo.delayMs.isEmpty()) {
        QString value;
        readTrimmed(kRknpuDebugRoot + QStringLiteral("/delayms"), &value, &errors);
        if (!value.isEmpty()) {
            m_npuStaticInfo.delayMs = formatDelayMs(value);
        }
    }

    if (m_npuStaticInfo.voltage.isEmpty()) {
        QString value;
        readTrimmed(kRknpuDebugRoot + QStringLiteral("/volt"), &value, &errors);
        if (!value.isEmpty()) {
            m_npuStaticInfo.voltage = formatVoltageFromMicroVolts(value);
        }
    }

    if (m_npuStaticInfo.version.isEmpty()) {
        QString value;
        readTrimmed(kRknpuDebugRoot + QStringLiteral("/version"), &value, &errors);
        if (!value.isEmpty()) {
            m_npuStaticInfo.version = value;
        }
    }

    const bool hasAny = !m_npuStaticInfo.frequency.isEmpty() ||
                        !m_npuStaticInfo.powerState.isEmpty() ||
                        !m_npuStaticInfo.delayMs.isEmpty() ||
                        !m_npuStaticInfo.voltage.isEmpty() ||
                        !m_npuStaticInfo.version.isEmpty();

    m_npuStaticInfo.available = hasAny;
    if (!errors.isEmpty() && !hasAny) {
        m_npuStaticInfo.detail = errors.join(QStringLiteral("; "));
    } else if (hasAny) {
        m_npuStaticInfo.detail.clear();
    }
}

void PerformanceMonitor::sampleResourceUsage() {
    ResourceSnapshot snapshot;
    snapshot.timestamp = QDateTime::currentDateTime();

    const qint64 nowNs = steadyNowNs();

    // Process CPU usage
    quint64 processTicks = 0;
    if (QByteArray statContent; readFileContent(QStringLiteral("/proc/self/stat"), &statContent)) {
        const int rightParenIndex = statContent.lastIndexOf(')');
        if (rightParenIndex != -1 && rightParenIndex + 2 < statContent.size()) {
            QByteArray after = statContent.mid(rightParenIndex + 2);
            QList<QByteArray> fields = after.split(' ');
            if (fields.size() >= 15) {
                bool ok1 = false;
                bool ok2 = false;
                quint64 utime = fields.value(11).toULongLong(&ok1);
                quint64 stime = fields.value(12).toULongLong(&ok2);
                if (ok1 && ok2) {
                    processTicks = utime + stime;
                }
            }
        }
    }

    quint64 totalTicks = 0;
    quint64 activeTicks = 0;
    if (QByteArray cpuContent; readFileContent(QStringLiteral("/proc/stat"), &cpuContent)) {
        QList<QByteArray> lines = cpuContent.split('\n');
        if (!lines.isEmpty()) {
            QList<QByteArray> tokens = lines.first().split(' ');
            QList<quint64> values;
            for (const QByteArray& token : tokens) {
                if (token.isEmpty() || token == "cpu") {
                    continue;
                }
                bool ok = false;
                quint64 val = token.toULongLong(&ok);
                if (ok) {
                    values.append(val);
                }
            }
            if (values.size() >= 7) {
                quint64 user = values.value(0);
                quint64 nice = values.value(1);
                quint64 system = values.value(2);
                quint64 idle = values.value(3);
                quint64 iowait = values.value(4);
                quint64 irq = values.value(5);
                quint64 softirq = values.value(6);
                quint64 steal = values.size() > 7 ? values.value(7) : 0;

                totalTicks = user + nice + system + idle + iowait + irq + softirq + steal;
                activeTicks = totalTicks - idle - iowait;
            }
        }
    }

    const quint64 deltaProcess = processTicks - m_lastProcessTicks;
    const quint64 deltaTotal = totalTicks - m_lastTotalTicks;
    const quint64 deltaActive = activeTicks - m_lastActiveTicks;

    if (deltaTotal > 0) {
        snapshot.processCpuPercent = ticksToPercent(deltaProcess, deltaTotal);
        snapshot.totalCpuPercent = std::clamp(100.0 * static_cast<double>(deltaActive) / static_cast<double>(deltaTotal), 0.0, 100.0);
    }

    m_lastProcessTicks = processTicks;
    m_lastTotalTicks = totalTicks;
    m_lastActiveTicks = activeTicks;

    // Process memory (RSS)
    if (QByteArray statmContent; readFileContent(QStringLiteral("/proc/self/statm"), &statmContent)) {
        QList<QByteArray> tokens = statmContent.split(' ');
        if (tokens.size() >= 2) {
            bool ok = false;
            quint64 rssPages = tokens.at(1).toULongLong(&ok);
            if (ok) {
                const quint64 pageSizeKb = static_cast<quint64>(::sysconf(_SC_PAGESIZE) / 1024);
                snapshot.processMemoryKb = rssPages * pageSizeKb;
            }
        }
    }

    // System available memory
    if (QByteArray memInfoContent; readFileContent(QStringLiteral("/proc/meminfo"), &memInfoContent)) {
        const QList<QByteArray> lines = memInfoContent.split('\n');
        for (const QByteArray& line : lines) {
            if (line.startsWith("MemAvailable:")) {
                QList<QByteArray> parts = line.split(' ');
                for (const QByteArray& part : parts) {
                    if (part.isEmpty() || part == "MemAvailable:" || part == "kB") {
                        continue;
                    }
                    bool ok = false;
                    quint64 value = part.toULongLong(&ok);
                    if (ok) {
                        snapshot.systemAvailableMemoryKb = value;
                        break;
                    }
                }
                break;
            }
        }
    }

    auto readPercentageMetric = [this](const QStringList& candidatePaths,
                                       PerformanceMetricAvailability* metric,
                                       const QString& fallbackDetail) {
        if (!metric) {
            return;
        }

        QStringList errors;
        for (const QString& path : candidatePaths) {
            if (!QFileInfo::exists(path)) {
                continue;
            }

            QByteArray content;
            QString errorDetail;
            if (!PerformanceMonitor::readFileContent(path, &content, &errorDetail)) {
                if (!errorDetail.isEmpty()) {
                    errors.append(errorDetail);
                }
                continue;
            }

            double parsed = 0.0;
            if (tryParsePercentage(content, &parsed)) {
                metric->available = true;
                metric->value = parsed;
                metric->detail.clear();
                return;
            }

            errors.append(tr("无法解析 %1 内容").arg(path));
        }

        metric->available = false;
        metric->value = 0.0;
        if (!errors.isEmpty()) {
            metric->detail = errors.join(QStringLiteral("; "));
        } else {
            metric->detail = fallbackDetail;
        }
    };

    readPercentageMetric({QStringLiteral("/sys/kernel/debug/rknpu/load"),
                          QStringLiteral("/proc/rknpu/load"),
                          QStringLiteral("/sys/class/devfreq/rknpu/load")},
                         &snapshot.npuLoad,
                         tr("未检测到可用的 NPU 占用率接口"));

    readPercentageMetric({QStringLiteral("/sys/class/devfreq/ff9a0000.gpu/load"),
                          QStringLiteral("/sys/class/devfreq/ff9a0000.gpu/utilization"),
                          QStringLiteral("/sys/kernel/debug/mali0/load")},
                         &snapshot.gpuLoad,
                         tr("GPU 驱动未提供占用率数据或驱动未加载"));

    PerformanceMetricAvailability rgaMetric;
    QList<QPair<QString, double>> rgaCoreLoads;
    const QString rgaDebugRoot = QStringLiteral("/sys/kernel/debug/rkrga");
    const QString rgaLoadPath = rgaDebugRoot + QStringLiteral("/load");
    
    // Read RGA Version
    if (QByteArray versionContent; readFileContent(rgaDebugRoot + QStringLiteral("/driver_version"), &versionContent)) {
        snapshot.rgaDriverVersion = QString::fromUtf8(versionContent).trimmed();
    }

    // Read RGA Hardware Info
    if (QByteArray hwContent; readFileContent(rgaDebugRoot + QStringLiteral("/hardware"), &hwContent)) {
        snapshot.rgaHardwareInfo = QString::fromUtf8(hwContent).trimmed();
    }

    if (QFileInfo::exists(rgaLoadPath)) {
        QByteArray content;
        QString errorDetail;
        if (PerformanceMonitor::readFileContent(rgaLoadPath, &content, &errorDetail)) {
            rgaMetric.available = true;
            rgaMetric.detail.clear();
            
            // Parse the content
            // Example format:
            // num of scheduler = 1
            // ================= load ==================
            // scheduler[0]: rga2
            //      load = 0%
            
            QString contentStr = QString::fromUtf8(content);
            QStringList lines = contentStr.split('\n');
            
            double totalLoad = 0.0;
            int schedulerCount = 0;
            
            QString currentSchedulerName;
            
            for (const QString& line : lines) {
                QString trimmed = line.trimmed();
                if (trimmed.startsWith("scheduler[")) {
                    // Extract name, e.g., "scheduler[0]: rga2" -> "rga2" or keep full name
                    int colonIndex = trimmed.indexOf(':');
                    if (colonIndex != -1) {
                        currentSchedulerName = trimmed.mid(colonIndex + 1).trimmed();
                        if (currentSchedulerName.isEmpty()) {
                            currentSchedulerName = trimmed.left(colonIndex).trimmed();
                        }
                    } else {
                        currentSchedulerName = trimmed;
                    }
                } else if (trimmed.startsWith("load =")) {
                    // Extract load, e.g., "load = 0%"
                    QString valStr = trimmed.mid(6).trimmed(); // Skip "load ="
                    if (valStr.endsWith('%')) {
                        valStr.chop(1);
                    }
                    bool ok = false;
                    double load = valStr.toDouble(&ok);
                    if (ok && !currentSchedulerName.isEmpty()) {
                        rgaCoreLoads.append({currentSchedulerName, load});
                        totalLoad += load;
                        schedulerCount++;
                        currentSchedulerName.clear();
                    }
                }
            }
            
            snapshot.rgaCoreLoads = rgaCoreLoads;
            if (schedulerCount > 0) {
                rgaMetric.value = totalLoad / schedulerCount; 
            } else {
                rgaMetric.value = 0.0;
            }
        } else {
            rgaMetric.available = false;
            rgaMetric.detail = errorDetail;
        }
    } else if (QFileInfo::exists(QStringLiteral("/dev/rga"))) {
        rgaMetric.available = false;
        rgaMetric.detail = QStringLiteral("驱动未公开 RGA 占用率接口 (/sys/kernel/debug/rkrga/load 不存在)");
    } else {
        rgaMetric.available = false;
        rgaMetric.detail = QStringLiteral("未检测到 /dev/rga 设备节点");
    }
    snapshot.rgaLoad = rgaMetric;

    {
        QMutexLocker ctxLocker(&m_npuMutex);
        if (m_npuContext != 0) {
            rknn_mem_size memInfo{};
            const int ret = rknn_query(m_npuContext, RKNN_QUERY_MEM_SIZE, &memInfo, sizeof(memInfo));
            if (ret == RKNN_SUCC) {
                snapshot.npuMemory.available = true;
                snapshot.npuMemory.detail.clear();
                snapshot.npuMemory.modelWeightsKb = bytesToKilobytes(memInfo.total_weight_size);
                snapshot.npuMemory.internalBuffersKb = bytesToKilobytes(memInfo.total_internal_size);
                snapshot.npuMemory.dmaAllocatedKb = bytesToKilobytes(memInfo.total_dma_allocated_size);
                snapshot.npuMemory.totalSramKb = bytesToKilobytes(memInfo.total_sram_size);
                snapshot.npuMemory.freeSramKb = bytesToKilobytes(memInfo.free_sram_size);
            } else {
                snapshot.npuMemory.available = false;
                snapshot.npuMemory.modelWeightsKb = 0;
                snapshot.npuMemory.internalBuffersKb = 0;
                snapshot.npuMemory.dmaAllocatedKb = 0;
                snapshot.npuMemory.totalSramKb = 0;
                snapshot.npuMemory.freeSramKb = 0;
                snapshot.npuMemory.detail = tr("rknn_query(RKNN_QUERY_MEM_SIZE) 失败: %1").arg(ret);
            }
        } else {
            snapshot.npuMemory.available = false;
            snapshot.npuMemory.modelWeightsKb = 0;
            snapshot.npuMemory.internalBuffersKb = 0;
            snapshot.npuMemory.dmaAllocatedKb = 0;
            snapshot.npuMemory.totalSramKb = 0;
            snapshot.npuMemory.freeSramKb = 0;
            snapshot.npuMemory.detail = tr("NPU 模型未初始化");
        }
    }

    ensureNpuStaticInfo();

    {
        QMutexLocker infoLocker(&m_npuInfoMutex);
        snapshot.npuFrequency = m_npuStaticInfo.frequency;
        snapshot.npuPowerState = m_npuStaticInfo.powerState;
        snapshot.npuDelayMs = m_npuStaticInfo.delayMs;
        snapshot.npuVoltage = m_npuStaticInfo.voltage;
        snapshot.npuDriverVersion = m_npuStaticInfo.version;
        snapshot.npuStaticDetail = m_npuStaticInfo.detail;
    }

    // VPU/MPP information
    {
        VpuInfo vpuInfo;
        QStringList errors;
        
        // Read MPP version
        QByteArray versionContent;
        if (readFileContent(QStringLiteral("/proc/mpp_service/version"), &versionContent)) {
            QString version = QString::fromUtf8(versionContent).trimmed();
            // Extract commit hash and author from version string
            if (version.contains("author:")) {
                vpuInfo.mppVersion = version.left(100);  // Limit length
            } else {
                vpuInfo.mppVersion = version;
            }
        }
        
        // Read JPEG decoder info (used for camera preview)
        QByteArray jpegBuffers;
        if (readFileContent(QStringLiteral("/proc/mpp_service/jpegd/session_buffers"), &jpegBuffers)) {
            bool ok = false;
            quint64 buffers = QString::fromUtf8(jpegBuffers).trimmed().toULongLong(&ok);
            if (ok) {
                vpuInfo.jpegSessionBuffers = buffers;
                vpuInfo.jpegDecoderStatus = (buffers > 0) ? tr("活跃 (%1 buffers)").arg(buffers) : tr("空闲");
            }
        }
        
        // Read video decoder info
        QByteArray videoTasks;
        QByteArray videoBuffers;
        if (readFileContent(QStringLiteral("/proc/mpp_service/rkvdec0/task_count"), &videoTasks)) {
            bool ok = false;
            quint64 tasks = QString::fromUtf8(videoTasks).trimmed().toULongLong(&ok);
            if (ok) {
                vpuInfo.videoTaskCount = tasks;
            }
        }
        if (readFileContent(QStringLiteral("/proc/mpp_service/rkvdec0/session_buffers"), &videoBuffers)) {
            bool ok = false;
            quint64 buffers = QString::fromUtf8(videoBuffers).trimmed().toULongLong(&ok);
            if (ok) {
                vpuInfo.videoSessionBuffers = buffers;
                vpuInfo.videoDecoderStatus = (buffers > 0) ? tr("活跃 (%1 buffers)").arg(buffers) : tr("空闲");
            }
        }
        
        // Determine availability
        vpuInfo.available = !vpuInfo.mppVersion.isEmpty();
        if (!vpuInfo.available) {
            vpuInfo.detail = tr("未能读取 /proc/mpp_service 信息");
        }
        
        snapshot.vpuInfo = vpuInfo;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_latestResources = snapshot;
    }

    emit resourceMetricsUpdated(snapshot);

    {
        QMutexLocker locker(&m_mutex);
        pruneStaleFrames(nowNs);
    }
}

void PerformanceMonitor::pruneStaleFrames(qint64 nowNs) {
    const qint64 threshold = nowNs - kFrameTimeoutNs;
    for (auto it = m_pendingFrames.begin(); it != m_pendingFrames.end();) {
        if (it->second.captureTimestampNs > 0 && it->second.captureTimestampNs < threshold) {
            it = m_pendingFrames.erase(it);
        } else {
            ++it;
        }
    }
}

qint64 PerformanceMonitor::steadyNowNs() {
    return toNanoseconds(std::chrono::steady_clock::now());
}

bool PerformanceMonitor::readFileContent(const QString& path, QByteArray* out, QString* errorDetail) {
    if (errorDetail) {
        errorDetail->clear();
    }

    if (!out) {
        if (errorDetail) {
            *errorDetail = QObject::tr("未提供输出缓冲区");
        }
        return false;
    }

    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *out = file.readAll();
        return true;
    }

    if (errorDetail) {
        switch (file.error()) {
        case QFile::PermissionsError:
            *errorDetail = QObject::tr("读取 %1 权限不足").arg(path);
            break;
        case QFile::OpenError:
        case QFile::ReadError:
            *errorDetail = file.errorString();
            break;
        default:
            break;
        }
    }

    QProcess process;
    process.start(QStringLiteral("cat"), {path});
    if (!process.waitForFinished(200)) {
        process.kill();
        process.waitForFinished();
        if (errorDetail) {
            *errorDetail = QObject::tr("读取 %1 超时").arg(path);
        }
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorDetail) {
            const QString stderrOutput = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (stderrOutput.contains(QStringLiteral("Permission denied"), Qt::CaseInsensitive)) {
                *errorDetail = QObject::tr("读取 %1 权限不足").arg(path);
            } else if (!stderrOutput.isEmpty()) {
                *errorDetail = stderrOutput;
            } else {
                *errorDetail = QObject::tr("读取 %1 失败 (退出码 %2)").arg(path).arg(process.exitCode());
            }
        }
        return false;
    }

    *out = process.readAllStandardOutput();
    return true;
}

double PerformanceMonitor::ticksToPercent(quint64 deltaTicks, quint64 deltaTotalTicks) {
    if (deltaTotalTicks == 0) {
        return 0.0;
    }
    const double percent = (static_cast<double>(deltaTicks) / static_cast<double>(deltaTotalTicks)) * 100.0;
    // percent already reflects share across all CPUs; clamp to [0, 100].
    return std::clamp(percent, 0.0, 100.0);
}

void PerformanceMonitor::triggerNpuProfiling(int numInferences) {
    QMutexLocker locker(&m_npuMutex);
    m_pendingProfilingInferences = numInferences;
    m_npuPerfReports.clear();
    m_npuPerfReports.reserve(numInferences);
    
    locker.unlock();
    
    // Request profiling for the first inference
    if (numInferences > 0) {
        requestProfiling();
    }
}

QString PerformanceMonitor::captureSnapshot(const QString& baseDirectory) {
    // Generate timestamp for filename
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString filename = QStringLiteral("performance_snapshot_%1.log").arg(timestamp);
    
    // Determine save directory
    QString savePath;
    if (!baseDirectory.isEmpty()) {
        savePath = baseDirectory + QStringLiteral("/") + filename;
    } else {
        // Try current directory first
        savePath = filename;
        QFile testFile(savePath);
        if (!testFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // Fallback to user directory
            const QString homeDir = QDir::homePath();
            const QString logDir = homeDir + QStringLiteral("/.feiqchatroom/logs");
            QDir().mkpath(logDir);
            savePath = logDir + QStringLiteral("/") + filename;
        } else {
            testFile.close();
            testFile.remove();
        }
    }
    
    // Generate snapshot content
    const QString content = generateSnapshotContent();
    
    // Write to file
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << savePath;
        return QString();
    }
    
    QTextStream stream(&file);
    stream << content;
    file.close();
    
    return savePath;
}

QString PerformanceMonitor::generateSnapshotContent() const {
    QString content;
    QTextStream out(&content);
    
    const ResourceSnapshot resources = latestResourceSnapshot();
    const QVector<FrameTimings> frames = recentFrameHistory(5);
    
    out << "================================================================================\n";
    out << "Performance Snapshot Report\n";
    out << "================================================================================\n";
    out << "Timestamp:    " << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
    out << "Version:      " << getVersionInfo() << "\n";
    out << "System Info:  " << getSystemInfo() << "\n";
    out << "================================================================================\n\n";
    
    out << "[System Resource Usage]\n";
    out << QString("Process CPU:       %1%\n").arg(resources.processCpuPercent, 0, 'f', 1);
    out << QString("System CPU:        %1%\n").arg(resources.totalCpuPercent, 0, 'f', 1);
    out << QString("Process Memory:    %1 KB (%2 MB)\n")
        .arg(resources.processMemoryKb)
        .arg(resources.processMemoryKb / 1024.0, 0, 'f', 1);
    out << QString("System Avail Mem:  %1 KB (%2 MB)\n\n")
        .arg(resources.systemAvailableMemoryKb)
        .arg(resources.systemAvailableMemoryKb / 1024.0, 0, 'f', 1);
    
    out << "[NPU Information]\n";
    if (resources.npuLoad.available) {
        out << QString("Utilization:   %1%\n").arg(resources.npuLoad.value, 0, 'f', 1);
    } else {
        out << QString("Utilization:   N/A (%1)\n").arg(resources.npuLoad.detail);
    }
    out << "Frequency:     " << (resources.npuFrequency.isEmpty() ? "N/A" : resources.npuFrequency) << "\n";
    out << "Voltage:       " << (resources.npuVoltage.isEmpty() ? "N/A" : resources.npuVoltage) << "\n";
    out << "Power State:   " << (resources.npuPowerState.isEmpty() ? "N/A" : resources.npuPowerState) << "\n";
    out << "Delay:         " << (resources.npuDelayMs.isEmpty() ? "N/A" : resources.npuDelayMs) << "\n";
    out << "Driver Ver:    " << (resources.npuDriverVersion.isEmpty() ? "N/A" : resources.npuDriverVersion) << "\n\n";
    
    out << "[NPU Memory Usage]\n";
    if (resources.npuMemory.available) {
        out << QString("Model Weights:      %1 KB (%2 MB)\n")
            .arg(resources.npuMemory.modelWeightsKb)
            .arg(resources.npuMemory.modelWeightsKb / 1024.0, 0, 'f', 1);
        out << QString("Internal Buffers:   %1 KB (%2 MB)\n")
            .arg(resources.npuMemory.internalBuffersKb)
            .arg(resources.npuMemory.internalBuffersKb / 1024.0, 0, 'f', 1);
        out << QString("DMA Allocated:      %1 KB (%2 MB)\n")
            .arg(resources.npuMemory.dmaAllocatedKb)
            .arg(resources.npuMemory.dmaAllocatedKb / 1024.0, 0, 'f', 1);
        out << QString("Total SRAM:         %1 KB (%2 MB)\n")
            .arg(resources.npuMemory.totalSramKb)
            .arg(resources.npuMemory.totalSramKb / 1024.0, 0, 'f', 1);
        out << QString("Available SRAM:     %1 KB (%2 MB)\n\n")
            .arg(resources.npuMemory.freeSramKb)
            .arg(resources.npuMemory.freeSramKb / 1024.0, 0, 'f', 1);
    } else {
        out << QString("N/A: %1\n\n").arg(resources.npuMemory.detail);
    }
    
    out << "[RGA Information]\n";
    if (resources.rgaLoad.available) {
        out << QString("Utilization:   %1%\n").arg(resources.rgaLoad.value, 0, 'f', 1);
        if (!resources.rgaCoreLoads.isEmpty()) {
            out << "Core Loads:\n";
            for (const auto& pair : resources.rgaCoreLoads) {
                out << QString("  %1: %2%\n").arg(pair.first).arg(pair.second, 0, 'f', 1);
            }
        }
    } else {
        out << QString("Utilization:   N/A (%1)\n").arg(resources.rgaLoad.detail);
    }
    out << "Driver Ver:    " << (resources.rgaDriverVersion.isEmpty() ? "N/A" : resources.rgaDriverVersion) << "\n";
    out << "Hardware Info: " << (resources.rgaHardwareInfo.isEmpty() ? "N/A" : resources.rgaHardwareInfo) << "\n\n";
    
    out << "[GPU Information]\n";
    if (resources.gpuLoad.available) {
        out << QString("Utilization:   %1%\n\n").arg(resources.gpuLoad.value, 0, 'f', 1);
    } else {
        out << QString("Utilization:   N/A (%1)\n\n").arg(resources.gpuLoad.detail);
    }
    
    out << "[VPU/MPP Information]\n";
    if (resources.vpuInfo.available) {
        out << "MPP Version:   " << resources.vpuInfo.mppVersion << "\n";
        out << "JPEG Decoder:  " << (resources.vpuInfo.jpegDecoderStatus.isEmpty() ? "N/A" : resources.vpuInfo.jpegDecoderStatus) << "\n";
        if (resources.vpuInfo.jpegSessionBuffers > 0) {
            out << QString("  Active Buffers: %1\n").arg(resources.vpuInfo.jpegSessionBuffers);
        }
        out << "Video Decoder: " << (resources.vpuInfo.videoDecoderStatus.isEmpty() ? "N/A" : resources.vpuInfo.videoDecoderStatus) << "\n";
        if (resources.vpuInfo.videoTaskCount > 0) {
            out << QString("  Task Count:     %1\n").arg(resources.vpuInfo.videoTaskCount);
        }
        if (resources.vpuInfo.videoSessionBuffers > 0) {
            out << QString("  Active Buffers: %1\n").arg(resources.vpuInfo.videoSessionBuffers);
        }
    } else {
        out << QString("Status:        N/A (%1)\n").arg(resources.vpuInfo.detail);
    }
    out << "\n";
    
    out << "[Frame Processing Performance - Recent 5 Frames]\n";
    out << "-------------------------\n";
    out << QString("%1 | %2 | %3 | %4 | %5 | %6 | %7\n")
        .arg("FrameID", -8).arg("Capture(us)", -12).arg("Preproc(us)", -12)
        .arg("NPU(us)", -9).arg("Postproc(us)", -13).arg("Render(us)", -11).arg("Total(us)", -10);
    out << QString("%1-|-%2-|-%3-|-%4-|-%5-|-%6-|-%7\n")
        .arg(QString(8, '-')).arg(QString(12, '-')).arg(QString(12, '-'))
        .arg(QString(9, '-')).arg(QString(13, '-')).arg(QString(11, '-')).arg(QString(10, '-'));
    
    for (const auto& frame : frames) {
        out << QString("%1 | %2 | %3 | %4 | %5 | %6 | %7\n")
            .arg(frame.frameId, 8).arg(frame.captureToPreprocessUs, 12).arg(frame.preprocessUs, 12)
            .arg(frame.npuUs, 9).arg(frame.postprocessUs, 13).arg(frame.renderUs, 11).arg(frame.totalLatencyUs, 10);
    }
    out << "\n";
    
    if (!frames.isEmpty()) {
        const LatencyStats stats = calculateLatencyStats(frames);
        out << "[Latency Statistics]\n";
        out << QString("Average:  %1 us (%2 ms)\n").arg(stats.avgUs, 0, 'f', 0).arg(stats.avgUs / 1000.0, 0, 'f', 1);
        out << QString("Min:      %1 us (%2 ms)\n").arg(stats.minUs, 0, 'f', 0).arg(stats.minUs / 1000.0, 0, 'f', 1);
        out << QString("Max:      %1 us (%2 ms)\n").arg(stats.maxUs, 0, 'f', 0).arg(stats.maxUs / 1000.0, 0, 'f', 1);
        out << QString("P50:      %1 us (%2 ms)\n").arg(stats.p50Us, 0, 'f', 0).arg(stats.p50Us / 1000.0, 0, 'f', 1);
        out << QString("P95:      %1 us (%2 ms)\n").arg(stats.p95Us, 0, 'f', 0).arg(stats.p95Us / 1000.0, 0, 'f', 1);
        out << QString("P99:      %1 us (%2 ms)\n\n").arg(stats.p99Us, 0, 'f', 0).arg(stats.p99Us / 1000.0, 0, 'f', 1);
    }
    
    {
        QMutexLocker locker(&m_npuMutex);
        if (!m_npuPerfReports.isEmpty()) {
            out << "[NPU Operator-Level Performance Analysis - " << m_npuPerfReports.size() << " Inferences]\n";
            out << "================================================================================\n";
            for (int i = 0; i < m_npuPerfReports.size(); ++i) {
                out << "\nInference #" << (i + 1) << ":\n" << m_npuPerfReports.at(i) << "\n";
                if (i < m_npuPerfReports.size() - 1) {
                    out << "--------------------------------------------------------------------------------\n";
                }
            }
            out << "================================================================================\n\n";
        }
    }
    
    out << "================================================================================\n";
    out << "Snapshot End\n";
    out << "================================================================================\n";
    
    return content;
}

QString PerformanceMonitor::getVersionInfo() const {
    return QStringLiteral("FeiQ Chatroom v1.0.0");
}

QString PerformanceMonitor::getSystemInfo() const {
    QString info;
    
    QFile versionFile("/proc/version");
    if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(versionFile.readAll());
        QRegularExpression re("Linux version ([^ ]+)");
        QRegularExpressionMatch match = re.match(content);
        if (match.hasMatch()) {
            info = QStringLiteral("Kernel ") + match.captured(1);
        }
    }
    
    return info.isEmpty() ? QStringLiteral("Unknown System") : info;
}

PerformanceMonitor::LatencyStats PerformanceMonitor::calculateLatencyStats(const QVector<FrameTimings>& history) const {
    LatencyStats stats;
    if (history.isEmpty()) {
        return stats;
    }
    
    QVector<double> latencies;
    latencies.reserve(history.size());
    
    double sum = 0.0;
    stats.minUs = std::numeric_limits<double>::max();
    stats.maxUs = std::numeric_limits<double>::lowest();
    
    for (const auto& frame : history) {
        if (frame.totalLatencyUs > 0) {
            double latency = static_cast<double>(frame.totalLatencyUs);
            latencies.append(latency);
            sum += latency;
            stats.minUs = std::min(stats.minUs, latency);
            stats.maxUs = std::max(stats.maxUs, latency);
        }
    }
    
    if (latencies.isEmpty()) {
        stats.minUs = 0.0;
        stats.maxUs = 0.0;
        return stats;
    }
    
    stats.avgUs = sum / latencies.size();
    
    std::sort(latencies.begin(), latencies.end());
    const int size = latencies.size();
    
    auto percentile = [&latencies, size](double p) -> double {
        const double index = (p / 100.0) * (size - 1);
        const int lower = static_cast<int>(std::floor(index));
        const int upper = static_cast<int>(std::ceil(index));
        
        if (lower == upper) {
            return latencies[lower];
        }
        
        const double fraction = index - lower;
        return latencies[lower] * (1.0 - fraction) + latencies[upper] * fraction;
    };
    
    stats.p50Us = percentile(50.0);
    stats.p95Us = percentile(95.0);
    stats.p99Us = percentile(99.0);
    
    return stats;
}
