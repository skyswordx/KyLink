#ifndef PERFORMANCE_ANALYTICS_DIALOG_H
#define PERFORMANCE_ANALYTICS_DIALOG_H

#include <QDialog>
#include <QVector>

#include "backend/PerformanceMonitor.h"

class QLabel;
class QTableWidget;
class QTimer;

class PerformanceAnalyticsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit PerformanceAnalyticsDialog(QWidget* parent = nullptr);
    ~PerformanceAnalyticsDialog() override;

private slots:
    void onFrameMetricsUpdated(const PerformanceMonitor::FrameTimings& latest,
                               const QVector<PerformanceMonitor::FrameTimings>& history);
    void onResourceMetricsUpdated(const PerformanceMonitor::ResourceSnapshot& snapshot);
    void refreshStageMetrics();
    void refreshResourceMetrics();

private:
    void initializeUi();
    void applyFrameHistory(const QVector<PerformanceMonitor::FrameTimings>& history);
    void clearStageSection();
    void refreshHistoryTable(const QVector<PerformanceMonitor::FrameTimings>& history);
    void updateStageSection(const PerformanceMonitor::FrameTimings& latest,
                            const QVector<PerformanceMonitor::FrameTimings>& history);
    void updateResourceSection(const PerformanceMonitor::ResourceSnapshot& snapshot);

    QLabel* m_captureDelayLabel;
    QLabel* m_preprocessLabel;
    QLabel* m_npuLabel;
    QLabel* m_renderLabel;
    QLabel* m_totalLatencyLabel;
    QLabel* m_resourceTimestampLabel;
    QLabel* m_memoryLabel;
    QLabel* m_npuMemoryLabel;
    QLabel* m_processCpuLabel;
    QLabel* m_systemCpuLabel;
    QLabel* m_npuUtilLabel;
    QLabel* m_npuFreqLabel;
    QLabel* m_npuPowerLabel;
    QLabel* m_npuDelayLabel;
    QLabel* m_npuVoltLabel;
    QLabel* m_npuVersionLabel;
    QLabel* m_npuStatusLabel;
    QLabel* m_gpuUtilLabel;
    QLabel* m_rgaUtilLabel;
    QLabel* m_rgaVersionLabel;
    QLabel* m_rgaHwLabel;
    QTableWidget* m_historyTable;
    QTimer* m_stageRefreshTimer;
    QTimer* m_resourceRefreshTimer;
};

#endif // PERFORMANCE_ANALYTICS_DIALOG_H
