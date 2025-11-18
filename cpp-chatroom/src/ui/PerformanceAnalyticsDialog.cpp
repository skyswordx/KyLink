#include "ui/PerformanceAnalyticsDialog.h"

#include "backend/PerformanceMonitor.h"

#include <QAbstractItemView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
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

QString formatMemoryKb(quint64 kilobytes) {
    if (kilobytes == 0) {
        return QStringLiteral("—");
    }
    const double mib = static_cast<double>(kilobytes) / 1024.0;
    return QStringLiteral("%1 MiB").arg(QString::number(mib, 'f', mib >= 100.0 ? 1 : 2));
}

} // namespace

PerformanceAnalyticsDialog::PerformanceAnalyticsDialog(QWidget* parent)
    : QDialog(parent)
    , m_captureDelayLabel(nullptr)
    , m_preprocessLabel(nullptr)
    , m_npuLabel(nullptr)
    , m_renderLabel(nullptr)
    , m_totalLatencyLabel(nullptr)
    , m_memoryLabel(nullptr)
    , m_processCpuLabel(nullptr)
    , m_systemCpuLabel(nullptr)
    , m_npuUtilLabel(nullptr)
    , m_gpuUtilLabel(nullptr)
    , m_rgaUtilLabel(nullptr)
    , m_historyTable(nullptr) {
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

    const auto history = monitor->recentFrameHistory();
    if (!history.isEmpty()) {
        onFrameMetricsUpdated(history.last(), history);
    }
    onResourceMetricsUpdated(monitor->latestResourceSnapshot());
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

    m_memoryLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_processCpuLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_systemCpuLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_npuUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_gpuUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);
    m_rgaUtilLabel = new QLabel(QStringLiteral("—"), resourceGroup);

    resourceLayout->addWidget(new QLabel(tr("进程内存:"), resourceGroup), 0, 0);
    resourceLayout->addWidget(m_memoryLabel, 0, 1);
    resourceLayout->addWidget(new QLabel(tr("进程 CPU:"), resourceGroup), 1, 0);
    resourceLayout->addWidget(m_processCpuLabel, 1, 1);
    resourceLayout->addWidget(new QLabel(tr("系统 CPU:"), resourceGroup), 2, 0);
    resourceLayout->addWidget(m_systemCpuLabel, 2, 1);
    resourceLayout->addWidget(new QLabel(tr("NPU 利用率:"), resourceGroup), 3, 0);
    resourceLayout->addWidget(m_npuUtilLabel, 3, 1);
    resourceLayout->addWidget(new QLabel(tr("GPU 利用率:"), resourceGroup), 4, 0);
    resourceLayout->addWidget(m_gpuUtilLabel, 4, 1);
    resourceLayout->addWidget(new QLabel(tr("RGA 利用率:"), resourceGroup), 5, 0);
    resourceLayout->addWidget(m_rgaUtilLabel, 5, 1);

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
    updateStageSection(latest, history);
    refreshHistoryTable(history);
}

void PerformanceAnalyticsDialog::onResourceMetricsUpdated(const PerformanceMonitor::ResourceSnapshot& snapshot) {
    updateResourceSection(snapshot);
}

void PerformanceAnalyticsDialog::updateStageSection(const PerformanceMonitor::FrameTimings& latest,
                                                    const QVector<PerformanceMonitor::FrameTimings>& history) {
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
    m_memoryLabel->setText(formatMemoryKb(snapshot.processMemoryKb));
    m_processCpuLabel->setText(formatPercent(snapshot.processCpuPercent, true, QString()));
    m_systemCpuLabel->setText(formatPercent(snapshot.totalCpuPercent, true, QString()));
    m_npuUtilLabel->setText(formatPercent(snapshot.npuLoad.value, snapshot.npuLoad.available, snapshot.npuLoad.detail));
    m_gpuUtilLabel->setText(formatPercent(snapshot.gpuLoad.value, snapshot.gpuLoad.available, snapshot.gpuLoad.detail));
    m_rgaUtilLabel->setText(formatPercent(snapshot.rgaLoad.value, snapshot.rgaLoad.available, snapshot.rgaLoad.detail));
}

void PerformanceAnalyticsDialog::refreshHistoryTable(const QVector<PerformanceMonitor::FrameTimings>& history) {
    const int rowCount = std::min(history.size(), kHistoryDisplayLimit);
    m_historyTable->setRowCount(rowCount);

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
