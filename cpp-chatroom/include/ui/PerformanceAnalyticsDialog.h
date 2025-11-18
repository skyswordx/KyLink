#ifndef PERFORMANCE_ANALYTICS_DIALOG_H
#define PERFORMANCE_ANALYTICS_DIALOG_H

#include <QDialog>
#include <QVector>

#include "backend/PerformanceMonitor.h"

class QLabel;
class QTableWidget;

class PerformanceAnalyticsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit PerformanceAnalyticsDialog(QWidget* parent = nullptr);
    ~PerformanceAnalyticsDialog() override;

private slots:
    void onFrameMetricsUpdated(const PerformanceMonitor::FrameTimings& latest,
                               const QVector<PerformanceMonitor::FrameTimings>& history);
    void onResourceMetricsUpdated(const PerformanceMonitor::ResourceSnapshot& snapshot);

private:
    void initializeUi();
    void refreshHistoryTable(const QVector<PerformanceMonitor::FrameTimings>& history);
    void updateStageSection(const PerformanceMonitor::FrameTimings& latest,
                            const QVector<PerformanceMonitor::FrameTimings>& history);
    void updateResourceSection(const PerformanceMonitor::ResourceSnapshot& snapshot);

    QLabel* m_captureDelayLabel;
    QLabel* m_preprocessLabel;
    QLabel* m_npuLabel;
    QLabel* m_renderLabel;
    QLabel* m_totalLatencyLabel;
    QLabel* m_memoryLabel;
    QLabel* m_processCpuLabel;
    QLabel* m_systemCpuLabel;
    QLabel* m_npuUtilLabel;
    QLabel* m_gpuUtilLabel;
    QLabel* m_rgaUtilLabel;
    QTableWidget* m_historyTable;
};

#endif // PERFORMANCE_ANALYTICS_DIALOG_H
