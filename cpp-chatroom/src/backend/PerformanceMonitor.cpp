#include "backend/PerformanceMonitor.h"

#include <QtCore/QByteArray>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QMutexLocker>
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
constexpr int kResourceSampleIntervalMs = 1000;
constexpr qint64 kFrameTimeoutNs = static_cast<qint64>(5) * 1000 * 1000 * 1000; // 5 seconds

inline qint64 toNanoseconds(const std::chrono::steady_clock::time_point& tp) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count();
}

inline qint64 toMicroseconds(const std::chrono::steady_clock::duration& duration) {
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
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

        historyCopy.reserve(static_cast<int>(m_history.size()));
        for (const auto& item : m_history) {
            historyCopy.append(item);
        }
    }

    emit frameMetricsUpdated(summary, historyCopy);
}

QVector<PerformanceMonitor::FrameTimings> PerformanceMonitor::recentFrameHistory() const {
    QMutexLocker locker(&m_mutex);
    QVector<FrameTimings> historyCopy;
    historyCopy.reserve(static_cast<int>(m_history.size()));
    for (const auto& entry : m_history) {
        historyCopy.append(entry);
    }
    return historyCopy;
}

PerformanceMonitor::ResourceSnapshot PerformanceMonitor::latestResourceSnapshot() const {
    QMutexLocker locker(&m_mutex);
    return m_latestResources;
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

    auto detectLoad = [](const QStringList& candidatePaths, PerformanceMetricAvailability* metric) {
        if (!metric) {
            return;
        }
        for (const QString& path : candidatePaths) {
            if (!QFileInfo::exists(path)) {
                continue;
            }
            QByteArray content;
            if (!PerformanceMonitor::readFileContent(path, &content)) {
                continue;
            }
            double parsed = 0.0;
            if (tryParsePercentage(content, &parsed)) {
                metric->available = true;
                metric->value = parsed;
                metric->detail = QString();
                return;
            }
        }
        metric->available = false;
        metric->value = 0.0;
    };

    detectLoad({QStringLiteral("/sys/kernel/debug/rknpu/load"),
                QStringLiteral("/proc/rknpu/load"),
                QStringLiteral("/sys/class/devfreq/rknpu/load")},
               &snapshot.npuLoad);
    if (!snapshot.npuLoad.available) {
        snapshot.npuLoad.detail = QStringLiteral("未检测到可用的 NPU 占用率接口");
    }

    detectLoad({QStringLiteral("/sys/class/devfreq/ff9a0000.gpu/load"),
                QStringLiteral("/sys/class/devfreq/ff9a0000.gpu/utilization"),
                QStringLiteral("/sys/kernel/debug/mali0/load")},
               &snapshot.gpuLoad);
    if (!snapshot.gpuLoad.available) {
        snapshot.gpuLoad.detail = QStringLiteral("GPU 驱动未提供占用率数据或驱动未加载");
    }

    PerformanceMetricAvailability rgaMetric;
    if (QFileInfo::exists(QStringLiteral("/dev/rga"))) {
        rgaMetric.available = false;
        rgaMetric.detail = QStringLiteral("驱动未公开 RGA 占用率接口");
    } else {
        rgaMetric.available = false;
        rgaMetric.detail = QStringLiteral("未检测到 /dev/rga 设备节点");
    }
    snapshot.rgaLoad = rgaMetric;

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

bool PerformanceMonitor::readFileContent(const QString& path, QByteArray* out) {
    if (!out) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    *out = file.readAll();
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
