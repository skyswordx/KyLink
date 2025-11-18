#ifndef PERFORMANCE_MONITOR_H
#define PERFORMANCE_MONITOR_H

#include <QtCore/QDateTime>
#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QVector>
#include <QtCore/QString>

#include <cstdint>
#include <deque>
#include <unordered_map>

struct PerformanceMetricAvailability {
    bool available = false;
    double value = 0.0; // interpreted as percentage when available
    QString detail;
};

class PerformanceMonitor final : public QObject {
    Q_OBJECT

public:
    struct FrameTimings {
        quint64 frameId = 0;
        qint64 captureTimestampNs = 0;
        qint64 captureToPreprocessUs = -1;
        qint64 preprocessUs = -1;
        qint64 npuUs = -1;
        qint64 postprocessUs = -1;
        qint64 renderUs = -1;
        qint64 totalLatencyUs = -1;
    };

    struct ResourceSnapshot {
        QDateTime timestamp;
        double processCpuPercent = 0.0;
        double totalCpuPercent = 0.0;
        quint64 processMemoryKb = 0;
        quint64 systemAvailableMemoryKb = 0;
        PerformanceMetricAvailability npuLoad;
        PerformanceMetricAvailability gpuLoad;
        PerformanceMetricAvailability rgaLoad;
    };

    static PerformanceMonitor* instance();

    quint64 recordFrameCaptured(qint64 captureTimestampNs);
    void recordDetectionStages(quint64 frameId,
                               qint64 captureToPreprocessUs,
                               qint64 preprocessUs,
                               qint64 npuUs,
                               qint64 postprocessUs,
                               qint64 detectionCompleteTimestampNs);
    void recordRenderStage(quint64 frameId,
                           qint64 renderDurationUs,
                           qint64 renderCompleteTimestampNs);

    QVector<FrameTimings> recentFrameHistory() const;
    ResourceSnapshot latestResourceSnapshot() const;

signals:
    void frameMetricsUpdated(const PerformanceMonitor::FrameTimings& latest,
                             const QVector<PerformanceMonitor::FrameTimings>& history);
    void resourceMetricsUpdated(const PerformanceMonitor::ResourceSnapshot& snapshot);

private slots:
    void sampleResourceUsage();

private:
    struct FrameRecord {
        qint64 captureTimestampNs = 0;
        qint64 detectionCompleteTimestampNs = 0;
        qint64 renderCompleteTimestampNs = 0;
        qint64 captureToPreprocessUs = -1;
        qint64 preprocessUs = -1;
        qint64 npuUs = -1;
        qint64 postprocessUs = -1;
        qint64 renderUs = -1;
    };

    explicit PerformanceMonitor(QObject* parent = nullptr);

    void pruneStaleFrames(qint64 nowNs);

    static qint64 steadyNowNs();
    static bool readFileContent(const QString& path, QByteArray* out);

    static double ticksToPercent(quint64 deltaTicks, quint64 deltaTotalTicks);

    QTimer m_resourceTimer;
    mutable QMutex m_mutex;
    std::unordered_map<quint64, FrameRecord> m_pendingFrames;
    std::deque<FrameTimings> m_history;
    ResourceSnapshot m_latestResources;
    quint64 m_nextFrameId;

    // CPU sampling state
    quint64 m_lastProcessTicks;
    quint64 m_lastTotalTicks;
    quint64 m_lastActiveTicks;
};

#endif // PERFORMANCE_MONITOR_H
