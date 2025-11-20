#include "ui/CameraPreviewDialog.h"

#include "backend/PerformanceMonitor.h"
#include "npu/RgaPreprocessor.h"
#include "npu/YoloV5Runner.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaType>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QFontMetrics>

#include <atomic>
#include <chrono>
#include <mutex>

#include <gst/app/gstappsink.h>
#include <gst/allocators/gstdmabuf.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

#include <unistd.h>

namespace {

constexpr int kBusPollIntervalMs = 150;
constexpr gint kQueueLeakyDownstream = 2; // matches GstQueueLeaky::GST_QUEUE_LEAKY_DOWNSTREAM in gstqueue.h
constexpr const char* kAppsinkName = "npu_appsink";

} // namespace

class CameraPreviewDialog::DetectionWorker final : public QObject {
    Q_OBJECT

public:
    explicit DetectionWorker(QObject* parent = nullptr)
        : QObject(parent) {}

    bool loadModel(const QString& modelPath, QString* errorOut) {
        QString error;
        if (!runner_.loadModel(modelPath, errorOut ? errorOut : &error)) {
            return false;
        }
        preprocessorReady_ = preprocessor_.initialize(runner_.inputWidth(), runner_.inputHeight());
        if (!preprocessorReady_) {
            if (errorOut) {
                *errorOut = tr("初始化 RGA 预处理失败");
            }
            return false;
        }
        return true;
    }

public slots:
    void processFrame(const FramePacket& packet) {
        if (!runner_.isReady()) {
            emit inferenceError(tr("模型未加载"));
            return;
        }

        if (packet.width <= 0 || packet.height <= 0 || packet.data.isEmpty()) {
            emit inferenceError(tr("无效帧数据"));
            return;
        }

        const bool isNv12 = (packet.videoFormat == GST_VIDEO_FORMAT_NV12);
        cv::Mat frameBgr;
        // 移除 CPU 转换，仅在非 NV12 且非 RGA 路径时作为回退
        if (!isNv12) {
            frameBgr = cv::Mat(packet.height,
                               packet.width,
                               CV_8UC3,
                               const_cast<char*>(packet.data.constData()),
                               packet.stride).clone();
        }

        const auto preprocessStart = std::chrono::steady_clock::now();
        qint64 captureToPreprocessUs = -1;
        if (packet.captureTimestampNs > 0) {
            const qint64 preprocessStartNs = std::chrono::duration_cast<std::chrono::nanoseconds>(preprocessStart.time_since_epoch()).count();
            captureToPreprocessUs = (preprocessStartNs - packet.captureTimestampNs) / 1000;
        }

        if (!preprocessorReady_) {
            preprocessorReady_ = preprocessor_.initialize(runner_.inputWidth(), runner_.inputHeight());
            if (!preprocessorReady_) {
                emit inferenceError(tr("RGA 预处理未就绪"));
                return;
            }
        }

        RgaPreprocessor::Result prepResult;
        RgaPreprocessor::Result displayResult; // 新增：显示结果

        struct AutoCloseFd {
            int fd;
            ~AutoCloseFd() {
                if (fd >= 0) {
                    ::close(fd);
                }
            }
        } fdCloser{packet.dmaFd};

        if (isNv12) {
            RgaPreprocessor::Nv12Input nv12Input;
            nv12Input.data = reinterpret_cast<const uint8_t*>(packet.data.constData());
            nv12Input.width = packet.width;
            nv12Input.height = packet.height;
            nv12Input.yStride = packet.yStride;
            nv12Input.uvStride = packet.uvStride;
            nv12Input.yPlaneOffset = packet.yPlaneOffset;
            nv12Input.uvPlaneOffset = packet.uvPlaneOffset;
            nv12Input.dmaFd = packet.dmaFd;
            
            // 1. RGA -> NPU Input (640x640)
            prepResult = preprocessor_.processNv12(nv12Input);

            // 2. RGA -> Display Image (Scaled to e.g. 960x540 for UI performance)
            // 使用较小的分辨率可以显著降低 Qt 渲染 (QPixmap::fromImage + scaled) 的耗时
            // 保持 16:9 比例
            displayResult = preprocessor_.processNv12ToDisplay(nv12Input, 960, 540);

        } else {
            // 回退路径
            prepResult = preprocessor_.processBgr(frameBgr);
            // 对于 BGR 输入，直接 clone 一份用于显示（或者也应该用 RGA 缩放，这里简化处理）
            if (prepResult.success) {
                // 模拟 displayResult
                displayResult.success = true;
                displayResult.data = nullptr; // 标记需要特殊处理
            }
        }

        if (!prepResult.success) {
            emit inferenceError(tr("RGA 预处理失败: %1").arg(prepResult.error));
            return;
        }

        DetectResultGroup detections{};
        std::int64_t inferenceTimeMs = 0;
        QString error;
        YoloV5Runner::InferenceBreakdown breakdown;
        breakdown.preprocessUs = prepResult.durationUs;

        // 传入空的 originalFrameBgr 和 nullptr visualizedFrame，避免 YoloV5Runner 内部的 OpenCV 绘图和拷贝
        bool inferenceOk = runner_.inferFromRgbBuffer(cv::Mat(), // 空 Mat
                                                      packet.width, // 原始宽
                                                      packet.height, // 原始高
                                                      prepResult.data,
                                                      prepResult.stride,
                                                      prepResult.letterbox,
                                                      &detections,
                                                      &inferenceTimeMs,
                                                      nullptr, // 不生成 visualizedFrame
                                                      &error,
                                                      &breakdown);
        
        if (!inferenceOk) {
             // ... error handling ...
             emit inferenceError(error);
             return;
        }

        const auto detectionComplete = std::chrono::steady_clock::now();
        const qint64 detectionCompleteNs = std::chrono::duration_cast<std::chrono::nanoseconds>(detectionComplete.time_since_epoch()).count();
        PerformanceMonitor::instance()->recordDetectionStages(packet.frameId,
                                                              captureToPreprocessUs,
                                                              breakdown.preprocessUs,
                                                              breakdown.npuUs,
                                                              breakdown.postprocessUs,
                                                              detectionCompleteNs);

        // 准备显示图像
        QImage image;
        if (displayResult.success && displayResult.data) {
            // 使用 RGA 生成的 RGB 数据
            // 注意：QImage 默认不拷贝数据，所以需要 copy() 或者确保数据生命周期
            // 这里我们直接 copy() 一份发送给 UI 线程，这样 Worker 线程的 buffer 可以复用
            image = QImage(displayResult.data,
                           displayResult.width,
                           displayResult.height,
                           displayResult.stride,
                           QImage::Format_RGB888).copy();
        } else if (!frameBgr.empty()) {
             // 回退：BGR -> RGB
             cv::Mat rgb;
             cv::cvtColor(frameBgr, rgb, cv::COLOR_BGR2RGB);
             image = QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
        }

        if (!image.isNull()) {
            // 使用 QPainter 在 QImage 上绘制检测框
            // 需要将检测框坐标从 原始分辨率 映射到 显示分辨率
            QPainter painter(&image);
            painter.setPen(QPen(Qt::red, 2));
            painter.setFont(QFont("Arial", 10));

            const float scaleX = static_cast<float>(image.width()) / static_cast<float>(packet.width);
            const float scaleY = static_cast<float>(image.height()) / static_cast<float>(packet.height);

            for (int i = 0; i < detections.count; ++i) {
                const auto& det = detections.results[i];
                int x = static_cast<int>(det.box.left * scaleX);
                int y = static_cast<int>(det.box.top * scaleY);
                int w = static_cast<int>((det.box.right - det.box.left) * scaleX);
                int h = static_cast<int>((det.box.bottom - det.box.top) * scaleY);

                painter.drawRect(x, y, w, h);
                
                QString label = QString("%1 %2%").arg(det.name).arg(static_cast<int>(det.prop * 100));
                // 绘制文字背景
                QFontMetrics fm(painter.font());
                int textW = fm.horizontalAdvance(label);
                int textH = fm.height();
                painter.fillRect(x, y - textH, textW + 4, textH, Qt::red);
                painter.setPen(Qt::white);
                painter.drawText(x + 2, y - 2, label);
                painter.setPen(QPen(Qt::red, 2)); // 恢复画笔
            }
        }

        emit inferenceReady(packet.frameId,
                            image, // 已经是 copy 过的
                            detections.count,
                            static_cast<qint64>(inferenceTimeMs));
    }

signals:
    void inferenceReady(quint64 frameId, const QImage& image, int objectCount, qint64 inferenceTimeMs);
    void inferenceError(const QString& errorText);

private:
    YoloV5Runner runner_;
    RgaPreprocessor preprocessor_;
    bool preprocessorReady_ = false;
};

CameraPreviewDialog::CameraPreviewDialog(QWidget* parent)
    : QDialog(parent)
    , m_deviceCombo(nullptr)
    , m_startStopButton(nullptr)
    , m_statusLabel(nullptr)
    , m_videoLabel(nullptr)
    , m_busPollTimer(nullptr)
    , m_pipeline(nullptr)
    , m_appSink(nullptr)
    , m_bus(nullptr)
    , m_isPlaying(false)
    , m_processingFrame(false)
    , m_detectionThread(nullptr)
    , m_detectionWorker(nullptr)
{
    setWindowTitle(tr("摄像头预览"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(720, 520);

    qRegisterMetaType<FramePacket>("FramePacket");

    initializeUi();
    populateDevices();
    updateControls();
}

CameraPreviewDialog::~CameraPreviewDialog()
{
    stopPipeline();
    clearDevices();
    disposeDetectionWorker();
}

void CameraPreviewDialog::closeEvent(QCloseEvent* event)
{
    stopPipeline();
    event->accept();
}

void CameraPreviewDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateDisplayedPixmap();
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

    m_videoLabel = new QLabel(this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumHeight(360);
    m_videoLabel->setStyleSheet("background-color: #202020; color: #ffffff;");
    m_videoLabel->setText(tr("暂无视频帧"));

    m_statusLabel = new QLabel(tr("正在初始化摄像头列表..."), this);

    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(m_videoLabel, 1);
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

    QString modelError;
    if (!ensureDetectionWorker(&modelError)) {
        updateStatusText(modelError);
        return;
    }

    GstDevice* device = m_devices.at(index).handle;
    if (!device) {
        updateStatusText(tr("摄像头设备不可用"));
        return;
    }

    GstElement* source = gst_device_create_element(device, nullptr);
    GstElement* convert = gst_element_factory_make("videoconvert", "convert");
    GstElement* queue = gst_element_factory_make("queue", "queue");
    GstElement* capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    GstElement* appsink = gst_element_factory_make("appsink", kAppsinkName);

    if (!source || !convert || !queue || !capsfilter || !appsink) {
        updateStatusText(tr("无法创建 GStreamer 元件"));
        if (source) gst_object_unref(source);
        if (convert) gst_object_unref(convert);
        if (queue) gst_object_unref(queue);
        if (capsfilter) gst_object_unref(capsfilter);
        if (appsink) gst_object_unref(appsink);
        return;
    }

    g_object_set(queue, "leaky", kQueueLeakyDownstream, nullptr); // drop stale frames when UI lags
    GstCaps* caps = gst_caps_from_string("video/x-raw,format=NV12");
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    g_object_set(appsink, "emit-signals", FALSE, "sync", FALSE, "max-buffers", 1, "drop", TRUE, nullptr);
    GstAppSinkCallbacks callbacks = {nullptr, nullptr, &CameraPreviewDialog::onAppSinkNewSample};
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks, this, nullptr);

    m_pipeline = gst_pipeline_new("camera-preview-pipeline");
    if (!m_pipeline) {
        updateStatusText(tr("无法创建 GStreamer 管线"));
        gst_object_unref(source);
        gst_object_unref(convert);
        gst_object_unref(queue);
        gst_object_unref(capsfilter);
        gst_object_unref(appsink);
        return;
    }

    gst_bin_add_many(GST_BIN(m_pipeline), source, queue, convert, capsfilter, appsink, nullptr);
    const gboolean linked = gst_element_link_many(source, queue, convert, capsfilter, appsink, nullptr);
    if (!linked) {
        updateStatusText(tr("GStreamer 管线连接失败"));
        stopPipeline();
        return;
    }

    m_appSink = appsink;

    m_bus = gst_element_get_bus(m_pipeline);
    if (!m_bus) {
        updateStatusText(tr("无法获取 GStreamer 总线"));
        stopPipeline();
        return;
    }

    m_busPollTimer->start();
    m_processingFrame = false;

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
    m_processingFrame = false;

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

    m_appSink = nullptr;
    m_isPlaying = false;
    clearVideoLabel();
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

bool CameraPreviewDialog::ensureDetectionWorker(QString* errorOut)
{
    if (m_detectionWorker) {
        return true;
    }

    auto* worker = new DetectionWorker();
    const QString assetDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("npu_assets"));
    const QString modelPath = QDir(assetDir).filePath(QStringLiteral("yolov5s_relu.rknn"));

    QString loadError;
    if (!worker->loadModel(modelPath, &loadError)) {
        if (errorOut) {
            *errorOut = tr("加载模型失败: %1").arg(loadError);
        }
        delete worker;
        return false;
    }

    m_detectionThread = new QThread(this);
    worker->moveToThread(m_detectionThread);
    connect(m_detectionThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &CameraPreviewDialog::frameCaptured, worker, &DetectionWorker::processFrame, Qt::QueuedConnection);
    connect(worker, &DetectionWorker::inferenceReady, this, &CameraPreviewDialog::onInferenceFrameReady, Qt::QueuedConnection);
    connect(worker, &DetectionWorker::inferenceError, this, &CameraPreviewDialog::onInferenceError, Qt::QueuedConnection);
    m_detectionThread->start();

    m_detectionWorker = worker;
    m_processingFrame = false;
    return true;
}

void CameraPreviewDialog::disposeDetectionWorker()
{
    if (!m_detectionThread) {
        return;
    }

    m_detectionThread->quit();
    m_detectionThread->wait();
    delete m_detectionThread;
    m_detectionThread = nullptr;
    m_detectionWorker = nullptr;
    m_processingFrame = false;
}

void CameraPreviewDialog::clearVideoLabel()
{
    m_currentPixmap = QPixmap();
    if (m_videoLabel) {
        m_videoLabel->setText(tr("暂无视频帧"));
        m_videoLabel->setPixmap(QPixmap());
    }
}

void CameraPreviewDialog::updateDisplayedPixmap()
{
    if (!m_videoLabel) {
        return;
    }

    if (m_currentPixmap.isNull()) {
        return;
    }

    const QSize targetSize = m_videoLabel->size();
    const QPixmap scaled = m_currentPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_videoLabel->setPixmap(scaled);
    m_videoLabel->setText(QString());
}

void CameraPreviewDialog::onInferenceFrameReady(quint64 frameId, const QImage& image, int objectCount, qint64 inferenceTimeMs)
{
    const auto renderStart = std::chrono::steady_clock::now();

    m_processingFrame = false;

    if (image.isNull()) {
        return;
    }

    m_currentPixmap = QPixmap::fromImage(image);
    updateDisplayedPixmap();

    if (m_videoLabel) {
        m_videoLabel->setText(QString());
    }

    updateStatusText(tr("预览中 - 目标 %1, 推理 %2 ms").arg(objectCount).arg(inferenceTimeMs));

    const auto renderEnd = std::chrono::steady_clock::now();
    const qint64 renderDurationUs = std::chrono::duration_cast<std::chrono::microseconds>(renderEnd - renderStart).count();
    const qint64 renderCompleteNs = std::chrono::duration_cast<std::chrono::nanoseconds>(renderEnd.time_since_epoch()).count();
    PerformanceMonitor::instance()->recordRenderStage(frameId, renderDurationUs, renderCompleteNs);
}

void CameraPreviewDialog::onInferenceError(const QString& errorText)
{
    m_processingFrame = false;
    updateStatusText(tr("推理失败: %1").arg(errorText));
}

void CameraPreviewDialog::handleNewFrame(const FramePacket& packet)
{
    if (!m_detectionWorker || !m_detectionThread || !m_detectionThread->isRunning()) {
        if (packet.dmaFd >= 0) {
            ::close(packet.dmaFd);
        }
        m_processingFrame = false;
        return;
    }

    emit frameCaptured(packet);
}

GstFlowReturn CameraPreviewDialog::onAppSinkNewSample(GstAppSink* sink, gpointer userData)
{
    auto* self = static_cast<CameraPreviewDialog*>(userData);
    if (!self || self->m_processingFrame.load()) {
        GstSample* discardSample = gst_app_sink_pull_sample(sink);
        if (discardSample) {
            gst_sample_unref(discardSample);
        }
        return GST_FLOW_OK;
    }

    GstSample* sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        return GST_FLOW_OK;
    }

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstVideoInfo info;
    gst_video_info_init(&info);
    if (!gst_video_info_from_caps(&info, caps)) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    int dmaFd = -1;
    const guint memoryCount = gst_buffer_n_memory(buffer);
    for (guint i = 0; i < memoryCount; ++i) {
        GstMemory* mem = gst_buffer_peek_memory(buffer, i);
        if (mem && gst_is_dmabuf_memory(mem)) {
            int fd = gst_dmabuf_memory_get_fd(mem);
            if (fd >= 0) {
                dmaFd = fd;
                break;
            }
        }
    }

    struct ScopedFdCloser {
        int& fdRef;
        explicit ScopedFdCloser(int& ref) : fdRef(ref) {}
        ~ScopedFdCloser() {
            if (fdRef >= 0) {
                ::close(fdRef);
            }
        }
    } fdCloserGuard(dmaFd);

    GstMapInfo mapInfo;
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    FramePacket packet;
    packet.width = GST_VIDEO_INFO_WIDTH(&info);
    packet.height = GST_VIDEO_INFO_HEIGHT(&info);
    packet.videoFormat = GST_VIDEO_INFO_FORMAT(&info);
    packet.stride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
    const guint planeCount = GST_VIDEO_INFO_N_PLANES(&info);
    if (planeCount > 0) {
        packet.yStride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 0);
        packet.yPlaneOffset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 0);
    }
    if (planeCount > 1) {
        packet.uvStride = GST_VIDEO_INFO_PLANE_STRIDE(&info, 1);
        packet.uvPlaneOffset = GST_VIDEO_INFO_PLANE_OFFSET(&info, 1);
    }
    packet.data = QByteArray(reinterpret_cast<const char*>(mapInfo.data), static_cast<int>(mapInfo.size));
    packet.pts = static_cast<std::int64_t>(GST_BUFFER_PTS(buffer));
    packet.captureTimestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    packet.frameId = PerformanceMonitor::instance()->recordFrameCaptured(packet.captureTimestampNs);
    packet.dmaFd = dmaFd;
    dmaFd = -1;

    gst_buffer_unmap(buffer, &mapInfo);
    gst_sample_unref(sample);

    self->m_processingFrame = true;
    QMetaObject::invokeMethod(self,
                              [self, packet]() {
                                  self->handleNewFrame(packet);
                              },
                              Qt::QueuedConnection);

    return GST_FLOW_OK;
}

#include "CameraPreviewDialog.moc"

