#include "backend/PerformanceMonitor.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
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
    , m_lastActiveTicks(0) {

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
