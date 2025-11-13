#include "ui/CameraPreviewDialog.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <mutex>

#include <gst/gst.h>
#include <gst/video/videooverlay.h>

namespace {

constexpr int kBusPollIntervalMs = 150;
constexpr gint kQueueLeakyDownstream = 2; // matches GstQueueLeaky::GST_QUEUE_LEAKY_DOWNSTREAM in gstqueue.h

} // namespace

CameraPreviewDialog::CameraPreviewDialog(QWidget* parent)
    : QDialog(parent)
    , m_deviceCombo(nullptr)
    , m_startStopButton(nullptr)
    , m_statusLabel(nullptr)
    , m_videoWidget(nullptr)
    , m_busPollTimer(nullptr)
    , m_pipeline(nullptr)
    , m_videoSink(nullptr)
    , m_bus(nullptr)
    , m_isPlaying(false)
{
    setWindowTitle(tr("摄像头预览"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(720, 520);

    initializeUi();
    populateDevices();
    updateControls();
}

CameraPreviewDialog::~CameraPreviewDialog()
{
    stopPipeline();
    clearDevices();
}

void CameraPreviewDialog::closeEvent(QCloseEvent* event)
{
    stopPipeline();
    event->accept();
}

void CameraPreviewDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    bindOverlayWindow();
}

void CameraPreviewDialog::initializeUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* controlsLayout = new QHBoxLayout();
    auto* deviceLabel = new QLabel(tr("摄像头:"), this);
    m_deviceCombo = new QComboBox(this);
    m_startStopButton = new QPushButton(tr("开始预览"), this);

    controlsLayout->addWidget(deviceLabel);
    controlsLayout->addWidget(m_deviceCombo, 1);
    controlsLayout->addWidget(m_startStopButton);

    m_videoWidget = new QWidget(this);
    m_videoWidget->setAttribute(Qt::WA_NativeWindow);
    m_videoWidget->setMinimumHeight(360);

    m_statusLabel = new QLabel(tr("正在初始化摄像头列表..."), this);

    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(m_videoWidget, 1);
    mainLayout->addWidget(m_statusLabel);

    m_busPollTimer = new QTimer(this);
    m_busPollTimer->setInterval(kBusPollIntervalMs);

    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraPreviewDialog::onDeviceChanged);
    connect(m_startStopButton, &QPushButton::clicked,
            this, &CameraPreviewDialog::onStartStopClicked);
    connect(m_busPollTimer, &QTimer::timeout,
            this, &CameraPreviewDialog::processBusMessages);
}

void CameraPreviewDialog::populateDevices()
{
    ensureGStreamerInitialized();
    clearDevices();

    m_deviceCombo->clear();

    GstDeviceMonitor* monitor = gst_device_monitor_new();
    if (!monitor) {
        updateStatusText(tr("无法创建 GStreamer 设备监视器"));
        m_deviceCombo->setEnabled(false);
        m_startStopButton->setEnabled(false);
        return;
    }

    GstCaps* caps = gst_caps_new_empty_simple("video/x-raw");
    gst_device_monitor_add_filter(monitor, "Video/Source", caps);
    gst_caps_unref(caps);

    if (!gst_device_monitor_start(monitor)) {
        updateStatusText(tr("无法启动设备监视器"));
        gst_object_unref(monitor);
        m_deviceCombo->setEnabled(false);
        m_startStopButton->setEnabled(false);
        return;
    }

    GList* devices = gst_device_monitor_get_devices(monitor);
    for (GList* item = devices; item != nullptr; item = item->next) {
        auto* device = GST_DEVICE(item->data);
        if (!device) {
            continue;
        }

        gchar* name = gst_device_get_display_name(device);
        VideoDevice info;
        info.label = name ? QString::fromUtf8(name) : tr("未知摄像头");
        info.handle = GST_DEVICE(g_object_ref(device));
        m_devices.push_back(info);
        m_deviceCombo->addItem(info.label);

        if (name) {
            g_free(name);
        }
    }

    g_list_free_full(devices, gst_object_unref);
    gst_device_monitor_stop(monitor);
    gst_object_unref(monitor);

    const bool hasDevices = !m_devices.empty();
    if (hasDevices) {
        m_deviceCombo->setCurrentIndex(0);
        updateStatusText(tr("请选择摄像头并点击开始预览"));
    } else {
        updateStatusText(tr("未检测到可用的摄像头"));
    }

    m_deviceCombo->setEnabled(hasDevices);
    m_startStopButton->setEnabled(hasDevices);
}

void CameraPreviewDialog::onDeviceChanged(int index)
{
    if (isPipelineActive()) {
        startPipelineForIndex(index);
        return;
    }

    if (index >= 0 && index < static_cast<int>(m_devices.size())) {
        updateStatusText(tr("已选择 %1，点击开始预览").arg(m_devices.at(index).label));
    }
}

void CameraPreviewDialog::onStartStopClicked()
{
    const int index = m_deviceCombo->currentIndex();
    if (index < 0 || index >= static_cast<int>(m_devices.size())) {
        updateStatusText(tr("未检测到可用的摄像头"));
        updateControls();
        return;
    }

    if (isPipelineActive()) {
        stopPipeline();
        updateStatusText(tr("摄像头已停止"));
    } else {
        startPipelineForIndex(index);
    }

    updateControls();
}

void CameraPreviewDialog::processBusMessages()
{
    if (!m_bus) {
        return;
    }

    GstMessage* message = nullptr;
    while ((message = gst_bus_pop(m_bus)) != nullptr) {
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* err = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &err, &debug);
            const QString errorText = err ? QString::fromUtf8(err->message) : tr("未知错误");
            updateStatusText(tr("摄像头错误: %1").arg(errorText));
            g_clear_error(&err);
            g_free(debug);
            stopPipeline();
            updateControls();
            break;
        }
        case GST_MESSAGE_EOS:
            updateStatusText(tr("视频流已结束"));
            stopPipeline();
            updateControls();
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(m_pipeline)) {
                GstState oldState, newState, pending;
                gst_message_parse_state_changed(message, &oldState, &newState, &pending);
                if (newState == GST_STATE_PLAYING) {
                    updateStatusText(tr("预览中"));
                } else if (newState == GST_STATE_READY) {
                    updateStatusText(tr("摄像头已加载"));
                }
                m_isPlaying = (newState == GST_STATE_PLAYING);
                updateControls();
            }
            break;
        default:
            break;
        }

        gst_message_unref(message);
    }
}

void CameraPreviewDialog::clearDevices()
{
    for (auto& device : m_devices) {
        if (device.handle) {
            gst_object_unref(device.handle);
            device.handle = nullptr;
        }
    }
    m_devices.clear();
}

void CameraPreviewDialog::startPipelineForIndex(int index)
{
    stopPipeline();

    if (index < 0 || index >= static_cast<int>(m_devices.size())) {
        updateStatusText(tr("未检测到可用的摄像头"));
        return;
    }

    ensureGStreamerInitialized();

    GstDevice* device = m_devices.at(index).handle;
    if (!device) {
        updateStatusText(tr("摄像头设备不可用"));
        return;
    }

    GstElement* source = gst_device_create_element(device, nullptr);
    GstElement* convert = gst_element_factory_make("videoconvert", "convert");
    GstElement* queue = gst_element_factory_make("queue", "queue");
    GstElement* sink = gst_element_factory_make("ximagesink", "sink");

    if (!source || !convert || !sink || !queue) {
        updateStatusText(tr("无法创建 GStreamer 元件"));
        if (source) gst_object_unref(source);
        if (convert) gst_object_unref(convert);
        if (queue) gst_object_unref(queue);
        if (sink) gst_object_unref(sink);
        return;
    }

    g_object_set(queue, "leaky", kQueueLeakyDownstream, nullptr); // drop stale frames when UI lags
    g_object_set(sink, "sync", FALSE, nullptr);

    m_pipeline = gst_pipeline_new("camera-preview-pipeline");
    if (!m_pipeline) {
        updateStatusText(tr("无法创建 GStreamer 管线"));
        gst_object_unref(source);
        gst_object_unref(convert);
        gst_object_unref(queue);
        gst_object_unref(sink);
        return;
    }

    gst_bin_add_many(GST_BIN(m_pipeline), source, queue, convert, sink, nullptr);
    const gboolean linked = gst_element_link_many(source, queue, convert, sink, nullptr);
    if (!linked) {
        updateStatusText(tr("GStreamer 管线连接失败"));
        stopPipeline();
        return;
    }

    m_videoSink = sink;
    bindOverlayWindow();

    m_bus = gst_element_get_bus(m_pipeline);
    if (!m_bus) {
        updateStatusText(tr("无法获取 GStreamer 总线"));
        stopPipeline();
        return;
    }

    m_busPollTimer->start();

    const GstStateChangeReturn ret = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        updateStatusText(tr("无法启动摄像头"));
        stopPipeline();
        return;
    }

    m_isPlaying = true;
    updateStatusText(tr("正在打开摄像头..."));
    updateControls();
}

void CameraPreviewDialog::stopPipeline()
{
    m_busPollTimer->stop();

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
    }

    if (m_bus) {
        gst_object_unref(m_bus);
        m_bus = nullptr;
    }

    if (m_pipeline) {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    m_videoSink = nullptr;
    m_isPlaying = false;
}

void CameraPreviewDialog::updateControls()
{
    const bool hasDevices = !m_devices.empty();
    m_startStopButton->setEnabled(hasDevices);

    if (!hasDevices) {
        m_startStopButton->setText(tr("开始预览"));
        return;
    }

    m_startStopButton->setText(m_isPlaying ? tr("停止预览") : tr("开始预览"));
}

void CameraPreviewDialog::updateStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}

void CameraPreviewDialog::bindOverlayWindow()
{
    if (!m_videoSink || !GST_IS_VIDEO_OVERLAY(m_videoSink)) {
        return;
    }

    const WId windowId = m_videoWidget->winId();
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(m_videoSink), static_cast<guintptr>(windowId));
    gst_video_overlay_handle_events(GST_VIDEO_OVERLAY(m_videoSink), TRUE);
    gst_video_overlay_set_render_rectangle(GST_VIDEO_OVERLAY(m_videoSink), 0, 0,
                                           m_videoWidget->width(), m_videoWidget->height());
    gst_video_overlay_expose(GST_VIDEO_OVERLAY(m_videoSink));
}

bool CameraPreviewDialog::isPipelineActive() const
{
    return m_pipeline != nullptr && m_isPlaying;
}

void CameraPreviewDialog::ensureGStreamerInitialized()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []() {
        gst_init(nullptr, nullptr);
    });
}
