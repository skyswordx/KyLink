#include "ui/PerformanceAnalyticsDialog.h"

#include "backend/PerformanceMonitor.h"

#include <QAbstractItemView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kHistoryDisplayLimit = 60;

QString formatMilliseconds(qint64 microseconds) {
    if (microseconds < 0) {
        return QStringLiteral("—");
    }
    const double milliseconds = static_cast<double>(microseconds) / 1000.0;
    return QString::number(milliseconds, 'f', milliseconds >= 100.0 ? 1 : 2);
}

QString formatPercent(double value, bool available, const QString& fallbackDetail) {
    if (!available) {
        return fallbackDetail.isEmpty() ? QStringLiteral("未提供") : fallbackDetail;
    }
    return QStringLiteral("%1 %").arg(QString::number(value, 'f', value >= 100.0 ? 1 : 2));
}

QString formatMemoryDetailed(quint64 kilobytes) {
    const double mib = static_cast<double>(kilobytes) / 1024.0;
    return QStringLiteral("%1 MiB (%2 KB)")
        .arg(QString::number(mib, 'f', mib >= 100.0 ? 1 : 2), QString::number(kilobytes));
}

} // namespace

PerformanceAnalyticsDialog::PerformanceAnalyticsDialog(QWidget* parent)
    : QDialog(parent)
    , m_captureDelayLabel(nullptr)
    , m_preprocessLabel(nullptr)
    , m_npuLabel(nullptr)
    , m_renderLabel(nullptr)
    , m_totalLatencyLabel(nullptr)
    , m_resourceTimestampLabel(nullptr)
    , m_memoryLabel(nullptr)
    , m_npuMemoryLabel(nullptr)
    , m_processCpuLabel(nullptr)
    , m_systemCpuLabel(nullptr)
    , m_npuUtilLabel(nullptr)
    , m_npuFreqLabel(nullptr)
    , m_npuPowerLabel(nullptr)
    , m_npuDelayLabel(nullptr)
    , m_npuVoltLabel(nullptr)
    , m_npuVersionLabel(nullptr)
    , m_npuStatusLabel(nullptr)
    , m_gpuUtilLabel(nullptr)
    , m_rgaUtilLabel(nullptr)
    , m_historyTable(nullptr)
    , m_stageRefreshTimer(nullptr)
    , m_resourceRefreshTimer(nullptr) {
    setWindowTitle(tr("性能分析"));
    resize(780, 560);

    initializeUi();

    PerformanceMonitor* monitor = PerformanceMonitor::instance();
    connect(monitor,
            &PerformanceMonitor::frameMetricsUpdated,
            this,
            &PerformanceAnalyticsDialog::onFrameMetricsUpdated,
            Qt::QueuedConnection);
    connect(monitor,
            &PerformanceMonitor::resourceMetricsUpdated,
            this,
            &PerformanceAnalyticsDialog::onResourceMetricsUpdated,
            Qt::QueuedConnection);

    const auto history = monitor->recentFrameHistory(kHistoryDisplayLimit);
    if (!history.isEmpty()) {
        applyFrameHistory(history);
    } else {
        clearStageSection();
    }
    onResourceMetricsUpdated(monitor->latestResourceSnapshot());

    m_stageRefreshTimer = new QTimer(this);
    m_stageRefreshTimer->setInterval(500);
    connect(m_stageRefreshTimer, &QTimer::timeout, this, &PerformanceAnalyticsDialog::refreshStageMetrics);
    m_stageRefreshTimer->start();

    m_resourceRefreshTimer = new QTimer(this);
    m_resourceRefreshTimer->setInterval(1000);
    connect(m_resourceRefreshTimer, &QTimer::timeout, this, &PerformanceAnalyticsDialog::refreshResourceMetrics);
    m_resourceRefreshTimer->start();
}

PerformanceAnalyticsDialog::~PerformanceAnalyticsDialog() = default;

void PerformanceAnalyticsDialog::initializeUi() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* pipelineGroup = new QGroupBox(tr("流水线耗时"), this);
    auto* pipelineLayout = new QGridLayout(pipelineGroup);

    m_captureDelayLabel = new QLabel(QStringLiteral("—"), pipelineGroup);
    m_preprocessLabel = new QLabel(QStringLiteral("—"), pipelineGroup);
    m_npuLabel = new QLabel(QStringLiteral("—"), pipelineGroup);
    m_renderLabel = new QLabel(QStringLiteral("—"), pipelineGroup);
    m_totalLatencyLabel = new QLabel(QStringLiteral("—"), pipelineGroup);

    pipelineLayout->addWidget(new QLabel(tr("摄像头→预处理排队:"), pipelineGroup), 0, 0);
    pipelineLayout->addWidget(m_captureDelayLabel, 0, 1);
    pipelineLayout->addWidget(new QLabel(tr("视频帧预处理:"), pipelineGroup), 1, 0);
    pipelineLayout->addWidget(m_preprocessLabel, 1, 1);
    pipelineLayout->addWidget(new QLabel(tr("NPU 推理:"), pipelineGroup), 2, 0);
    pipelineLayout->addWidget(m_npuLabel, 2, 1);
    pipelineLayout->addWidget(new QLabel(tr("Qt 渲染:"), pipelineGroup), 3, 0);
    pipelineLayout->addWidget(m_renderLabel, 3, 1);
    pipelineLayout->addWidget(new QLabel(tr("端到端延迟:"), pipelineGroup), 4, 0);
    pipelineLayout->addWidget(m_totalLatencyLabel, 4, 1);

    pipelineLayout->setColumnStretch(1, 1);

    auto* resourceGroup = new QGroupBox(tr("系统资源"), this);
    auto* resourceLayout = new QGridLayout(resourceGroup);

    m_resourceTimestampLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_memoryLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuMemoryLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_processCpuLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_systemCpuLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuFreqLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuPowerLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuDelayLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuVoltLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuVersionLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuStatusLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuStatusLabel->setWordWrap(true);
    m_gpuUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_rgaUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);

    resourceLayout->addWidget(new QLabel(tr("采样时间:"), resourceGroup), 0, 0);
    resourceLayout->addWidget(m_resourceTimestampLabel, 0, 1);
    resourceLayout->addWidget(new QLabel(tr("进程内存:"), resourceGroup), 1, 0);
    resourceLayout->addWidget(m_memoryLabel, 1, 1);
    resourceLayout->addWidget(new QLabel(tr("进程 CPU:"), resourceGroup), 2, 0);
    resourceLayout->addWidget(m_processCpuLabel, 2, 1);
    resourceLayout->addWidget(new QLabel(tr("系统 CPU:"), resourceGroup), 3, 0);
    resourceLayout->addWidget(m_systemCpuLabel, 3, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 利用率:"), resourceGroup), 4, 0);
    resourceLayout->addWidget(m_npuUtilLabel, 4, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 频率:"), resourceGroup), 5, 0);
    resourceLayout->addWidget(m_npuFreqLabel, 5, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 电源状态:"), resourceGroup), 6, 0);
    resourceLayout->addWidget(m_npuPowerLabel, 6, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU Delayms:"), resourceGroup), 7, 0);
    resourceLayout->addWidget(m_npuDelayLabel, 7, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 电压:"), resourceGroup), 8, 0);
    resourceLayout->addWidget(m_npuVoltLabel, 8, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 驱动版本:"), resourceGroup), 9, 0);
    resourceLayout->addWidget(m_npuVersionLabel, 9, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 内存:"), resourceGroup), 10, 0);
    resourceLayout->addWidget(m_npuMemoryLabel, 10, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 调试信息:"), resourceGroup), 11, 0);
    resourceLayout->addWidget(m_npuStatusLabel, 11, 1);
    resourceLayout->addWidget(new QLabel(tr("GPU 利用率:"), resourceGroup), 12, 0);
    resourceLayout->addWidget(m_gpuUtilLabel, 12, 1);
    resourceLayout->addWidget(new QLabel(tr("RGA 利用率:"), resourceGroup), 13, 0);
    resourceLayout->addWidget(m_rgaUtilLabel, 13, 1);

    resourceLayout->setColumnStretch(1, 1);

    m_historyTable = new QTableWidget(this);
    m_historyTable->setColumnCount(7);
    m_historyTable->setHorizontalHeaderLabels({tr("Frame"),
                                               tr("摄像头→预处理 (ms)"),
                                               tr("预处理 (ms)"),
                                               tr("NPU (ms)"),
                                               tr("后处理 (ms)"),
                                               tr("Qt 渲染 (ms)"),
                                               tr("端到端 (ms)")});
    m_historyTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_historyTable->verticalHeader()->setVisible(false);

    mainLayout->addWidget(pipelineGroup);
    mainLayout->addWidget(resourceGroup);
    mainLayout->addWidget(m_historyTable, 1);
}

void PerformanceAnalyticsDialog::onFrameMetricsUpdated(const PerformanceMonitor::FrameTimings& latest,
                                                       const QVector<PerformanceMonitor::FrameTimings>& history) {
    Q_UNUSED(latest);
    applyFrameHistory(history);
}

void PerformanceAnalyticsDialog::onResourceMetricsUpdated(const PerformanceMonitor::ResourceSnapshot& snapshot) {
    updateResourceSection(snapshot);
}

void PerformanceAnalyticsDialog::refreshStageMetrics() {
    PerformanceMonitor* monitor = PerformanceMonitor::instance();
    applyFrameHistory(monitor->recentFrameHistory(kHistoryDisplayLimit));
}

void PerformanceAnalyticsDialog::refreshResourceMetrics() {
    PerformanceMonitor* monitor = PerformanceMonitor::instance();
    updateResourceSection(monitor->latestResourceSnapshot());
}

void PerformanceAnalyticsDialog::updateStageSection(const PerformanceMonitor::FrameTimings& latest,
                                                    const QVector<PerformanceMonitor::FrameTimings>& history) {
    if (history.isEmpty()) {
        clearStageSection();
        return;
    }

    auto computeAverage = [](const QVector<PerformanceMonitor::FrameTimings>& samples,
                             auto accessor) -> QString {
        qint64 sum = 0;
        int count = 0;
        for (const auto& sample : samples) {
            const qint64 value = accessor(sample);
            if (value >= 0) {
                sum += value;
                ++count;
            }
        }
        if (count == 0) {
            return QStringLiteral("—");
        }
        return formatMilliseconds(sum / count);
    };

    const QString avgCaptureDelay = computeAverage(history, [](const auto& item) { return item.captureToPreprocessUs; });
    const QString avgPreprocess = computeAverage(history, [](const auto& item) { return item.preprocessUs; });
    const QString avgNpu = computeAverage(history, [](const auto& item) { return item.npuUs; });
    const QString avgRender = computeAverage(history, [](const auto& item) { return item.renderUs; });
    const QString avgLatency = computeAverage(history, [](const auto& item) { return item.totalLatencyUs; });

    const QString captureLatest = formatMilliseconds(latest.captureToPreprocessUs);
    const QString preprocessLatest = formatMilliseconds(latest.preprocessUs);
    const QString npuLatest = formatMilliseconds(latest.npuUs);
    const QString renderLatest = formatMilliseconds(latest.renderUs);
    const QString latencyLatest = formatMilliseconds(latest.totalLatencyUs);

    m_captureDelayLabel->setText(tr("%1 ms (平均 %2 ms)").arg(captureLatest, avgCaptureDelay));
    m_preprocessLabel->setText(tr("%1 ms (平均 %2 ms)").arg(preprocessLatest, avgPreprocess));
    m_npuLabel->setText(tr("%1 ms (平均 %2 ms)").arg(npuLatest, avgNpu));
    m_renderLabel->setText(tr("%1 ms (平均 %2 ms)").arg(renderLatest, avgRender));
    m_totalLatencyLabel->setText(tr("%1 ms (平均 %2 ms)").arg(latencyLatest, avgLatency));
}

void PerformanceAnalyticsDialog::updateResourceSection(const PerformanceMonitor::ResourceSnapshot& snapshot) {
    m_resourceTimestampLabel->setText(snapshot.timestamp.toString(QStringLiteral("HH:mm:ss")));
    m_memoryLabel->setText(formatMemoryDetailed(snapshot.processMemoryKb));
    m_processCpuLabel->setText(formatPercent(snapshot.processCpuPercent, true, QString()));
    m_systemCpuLabel->setText(formatPercent(snapshot.totalCpuPercent, true, QString()));
    m_npuUtilLabel->setText(formatPercent(snapshot.npuLoad.value, snapshot.npuLoad.available, snapshot.npuLoad.detail));
    auto setOrDash = [](QLabel* label, const QString& text) {
        if (!label) {
            return;
        }
        label->setText(text.isEmpty() ? QStringLiteral("—") : text);
    };

    setOrDash(m_npuFreqLabel, snapshot.npuFrequency);
    setOrDash(m_npuPowerLabel, snapshot.npuPowerState);
    setOrDash(m_npuDelayLabel, snapshot.npuDelayMs);
    setOrDash(m_npuVoltLabel, snapshot.npuVoltage);
    setOrDash(m_npuVersionLabel, snapshot.npuDriverVersion);

    if (snapshot.npuMemory.available) {
        m_npuMemoryLabel->setText(tr("模型 %1 · 内部 %2 · DMA %3 · SRAM 空余 %4 / %5")
                                      .arg(formatMemoryDetailed(snapshot.npuMemory.modelWeightsKb),
                                           formatMemoryDetailed(snapshot.npuMemory.internalBuffersKb),
                                           formatMemoryDetailed(snapshot.npuMemory.dmaAllocatedKb),
                                           formatMemoryDetailed(snapshot.npuMemory.freeSramKb),
                                           formatMemoryDetailed(snapshot.npuMemory.totalSramKb)));
    } else {
        setOrDash(m_npuMemoryLabel, snapshot.npuMemory.detail);
    }

    setOrDash(m_npuStatusLabel, snapshot.npuStaticDetail);
    m_gpuUtilLabel->setText(formatPercent(snapshot.gpuLoad.value, snapshot.gpuLoad.available, snapshot.gpuLoad.detail));
    m_rgaUtilLabel->setText(formatPercent(snapshot.rgaLoad.value, snapshot.rgaLoad.available, snapshot.rgaLoad.detail));
}

void PerformanceAnalyticsDialog::refreshHistoryTable(const QVector<PerformanceMonitor::FrameTimings>& history) {
    const int rowCount = std::min(history.size(), kHistoryDisplayLimit);
    m_historyTable->setRowCount(rowCount);
    if (rowCount == 0) {
        return;
    }

    int row = 0;
    for (int i = history.size() - 1; i >= 0 && row < rowCount; --i, ++row) {
        const auto& item = history.at(i);
        auto setCell = [this, row](int column, const QString& text) {
            auto* cell = new QTableWidgetItem(text);
            cell->setTextAlignment(Qt::AlignCenter);
            m_historyTable->setItem(row, column, cell);
        };

        setCell(0, QString::number(item.frameId));
        setCell(1, formatMilliseconds(item.captureToPreprocessUs));
        setCell(2, formatMilliseconds(item.preprocessUs));
        setCell(3, formatMilliseconds(item.npuUs));
        setCell(4, formatMilliseconds(item.postprocessUs));
        setCell(5, formatMilliseconds(item.renderUs));
        setCell(6, formatMilliseconds(item.totalLatencyUs));
    }
}

void PerformanceAnalyticsDialog::applyFrameHistory(const QVector<PerformanceMonitor::FrameTimings>& history) {
    if (history.isEmpty()) {
        clearStageSection();
        return;
    }

    updateStageSection(history.last(), history);
    refreshHistoryTable(history);
}

void PerformanceAnalyticsDialog::clearStageSection() {
    const QString dash = QStringLiteral("—");
    const QString placeholder = tr("%1 ms (平均 %2 ms)").arg(dash, dash);
    m_captureDelayLabel->setText(placeholder);
    m_preprocessLabel->setText(placeholder);
    m_npuLabel->setText(placeholder);
    m_renderLabel->setText(placeholder);
    m_totalLatencyLabel->setText(placeholder);
    m_historyTable->setRowCount(0);
}
