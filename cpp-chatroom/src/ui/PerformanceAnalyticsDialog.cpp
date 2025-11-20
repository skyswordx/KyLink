#include "ui/PerformanceAnalyticsDialog.h"

#include "backend/PerformanceMonitor.h"

#include <QAbstractItemView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
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
    
    // NPU Details Widget (Collapsible)
    auto* npuDetailsWidget = new QWidget(resourceGroup);
    auto* npuDetailsLayout = new QGridLayout(npuDetailsWidget);
    npuDetailsLayout->setContentsMargins(0, 0, 0, 0);
    
    m_npuFreqLabel = new QLabel(QStringLiteral("—"), npuDetailsWidget);
    m_npuPowerLabel = new QLabel(QStringLiteral("—"), npuDetailsWidget);
    m_npuDelayLabel = new QLabel(QStringLiteral("—"), npuDetailsWidget);
    m_npuVoltLabel = new QLabel(QStringLiteral("—"), npuDetailsWidget);
    m_npuVersionLabel = new QLabel(QStringLiteral("—"), npuDetailsWidget);
    m_npuStatusLabel = new QLabel(QStringLiteral("—"), npuDetailsWidget);
    m_npuStatusLabel->setWordWrap(true);

    npuDetailsLayout->addWidget(new QLabel(tr("NPU 频率:"), npuDetailsWidget), 0, 0);
    npuDetailsLayout->addWidget(m_npuFreqLabel, 0, 1);
    npuDetailsLayout->addWidget(new QLabel(tr("NPU 电源状态:"), npuDetailsWidget), 1, 0);
    npuDetailsLayout->addWidget(m_npuPowerLabel, 1, 1);
    npuDetailsLayout->addWidget(new QLabel(tr("NPU Delayms:"), npuDetailsWidget), 2, 0);
    npuDetailsLayout->addWidget(m_npuDelayLabel, 2, 1);
    npuDetailsLayout->addWidget(new QLabel(tr("NPU 电压:"), npuDetailsWidget), 3, 0);
    npuDetailsLayout->addWidget(m_npuVoltLabel, 3, 1);
    npuDetailsLayout->addWidget(new QLabel(tr("NPU 驱动版本:"), npuDetailsWidget), 4, 0);
    npuDetailsLayout->addWidget(m_npuVersionLabel, 4, 1);
    npuDetailsLayout->addWidget(new QLabel(tr("NPU 内存:"), npuDetailsWidget), 5, 0);
    npuDetailsLayout->addWidget(m_npuMemoryLabel, 5, 1);
    npuDetailsLayout->addWidget(new QLabel(tr("NPU 调试信息:"), npuDetailsWidget), 6, 0);
    npuDetailsLayout->addWidget(m_npuStatusLabel, 6, 1);
    
    npuDetailsLayout->setColumnStretch(1, 1);
    npuDetailsWidget->setVisible(false); // Default hidden

    auto* toggleNpuDetailsBtn = new QPushButton(tr("显示 NPU 详细信息"), resourceGroup);
    toggleNpuDetailsBtn->setCheckable(true);
    connect(toggleNpuDetailsBtn, &QPushButton::toggled, [toggleNpuDetailsBtn, npuDetailsWidget](bool checked) {
        npuDetailsWidget->setVisible(checked);
        toggleNpuDetailsBtn->setText(checked ? tr("隐藏 NPU 详细信息") : tr("显示 NPU 详细信息"));
    });

    m_gpuUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_rgaUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_rgaUtilLabel->setWordWrap(true); // Allow multi-line for multiple RGA cores

    int row = 0;
    resourceLayout->addWidget(new QLabel(tr("采样时间:"), resourceGroup), row, 0);
    resourceLayout->addWidget(m_resourceTimestampLabel, row++, 1);
    resourceLayout->addWidget(new QLabel(tr("进程内存:"), resourceGroup), row, 0);
    resourceLayout->addWidget(m_memoryLabel, row++, 1);
    resourceLayout->addWidget(new QLabel(tr("进程 CPU:"), resourceGroup), row, 0);
    resourceLayout->addWidget(m_processCpuLabel, row++, 1);
    resourceLayout->addWidget(new QLabel(tr("系统 CPU:"), resourceGroup), row, 0);
    resourceLayout->addWidget(m_systemCpuLabel, row++, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 利用率:"), resourceGroup), row, 0);
    resourceLayout->addWidget(m_npuUtilLabel, row++, 1);
    
    resourceLayout->addWidget(toggleNpuDetailsBtn, row++, 0, 1, 2);
    resourceLayout->addWidget(npuDetailsWidget, row++, 0, 1, 2);

    resourceLayout->addWidget(new QLabel(tr("GPU 利用率:"), resourceGroup), row, 0);
    resourceLayout->addWidget(m_gpuUtilLabel, row++, 1);
    resourceLayout->addWidget(new QLabel(tr("RGA 利用率:"), resourceGroup), row, 0);
    resourceLayout->addWidget(m_rgaUtilLabel, row++, 1);

    // RGA Details Widget (Collapsible)
    auto* rgaDetailsWidget = new QWidget(resourceGroup);
    auto* rgaDetailsLayout = new QGridLayout(rgaDetailsWidget);
    rgaDetailsLayout->setContentsMargins(0, 0, 0, 0);

    m_rgaVersionLabel = new QLabel(QStringLiteral("—"), rgaDetailsWidget);
    m_rgaHwLabel = new QLabel(QStringLiteral("—"), rgaDetailsWidget);
    m_rgaHwLabel->setWordWrap(true);

    rgaDetailsLayout->addWidget(new QLabel(tr("RGA 驱动版本:"), rgaDetailsWidget), 0, 0);
    rgaDetailsLayout->addWidget(m_rgaVersionLabel, 0, 1);
    rgaDetailsLayout->addWidget(new QLabel(tr("RGA 硬件信息:"), rgaDetailsWidget), 1, 0);
    rgaDetailsLayout->addWidget(m_rgaHwLabel, 1, 1);

    rgaDetailsLayout->setColumnStretch(1, 1);
    rgaDetailsWidget->setVisible(false);

    auto* toggleRgaDetailsBtn = new QPushButton(tr("显示 RGA 详细信息"), resourceGroup);
    toggleRgaDetailsBtn->setCheckable(true);
    connect(toggleRgaDetailsBtn, &QPushButton::toggled, [toggleRgaDetailsBtn, rgaDetailsWidget](bool checked) {
        rgaDetailsWidget->setVisible(checked);
        toggleRgaDetailsBtn->setText(checked ? tr("隐藏 RGA 详细信息") : tr("显示 RGA 详细信息"));
    });

    resourceLayout->addWidget(toggleRgaDetailsBtn, row++, 0, 1, 2);
    resourceLayout->addWidget(rgaDetailsWidget, row++, 0, 1, 2);

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
    
    if (!snapshot.rgaCoreLoads.isEmpty()) {
        QStringList rgaDetails;
        for (const auto& pair : snapshot.rgaCoreLoads) {
            rgaDetails.append(QStringLiteral("%1: %2%").arg(pair.first, QString::number(pair.second, 'f', 1)));
        }
        m_rgaUtilLabel->setText(rgaDetails.join(QStringLiteral("\n")));
    } else {
        m_rgaUtilLabel->setText(formatPercent(snapshot.rgaLoad.value, snapshot.rgaLoad.available, snapshot.rgaLoad.detail));
    }

    setOrDash(m_rgaVersionLabel, snapshot.rgaDriverVersion);
    setOrDash(m_rgaHwLabel, snapshot.rgaHardwareInfo);
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
