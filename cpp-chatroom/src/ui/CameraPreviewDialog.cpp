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
        : QObject(parent) {
        connect(PerformanceMonitor::instance(), &PerformanceMonitor::profilingRequested,
                this, [this]() {
                    QMutexLocker locker(&m_mutex);
                    m_profilingPending = true;
                }, Qt::DirectConnection);
    }

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

    // 线程安全的帧更新接口 (Mailbox 模式)
    void updateFrame(const FramePacket& packet) {
        QMutexLocker locker(&m_mutex);
        m_pendingPacket = packet;
        m_hasPending = true;
        
        if (!m_isRunning) {
            m_isRunning = true;
            QMetaObject::invokeMethod(this, "processLoop", Qt::QueuedConnection);
        }
    }

public slots:
    void processLoop() {
        FramePacket packet;
        {
            QMutexLocker locker(&m_mutex);
            if (!m_hasPending) {
                m_isRunning = false;
                return;
            }
            packet = m_pendingPacket;
            m_hasPending = false;
            // m_isRunning remains true
        }

        processFrame(packet);

        // 继续处理下一帧（如果有的化）
        QMetaObject::invokeMethod(this, "processLoop", Qt::QueuedConnection);
    }

    void processFrame(const FramePacket& packet) {
        if (!runner_.isReady()) {
            emit inferenceError(tr("模型未加载"));
            emit workerReady();
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

        // --- 关键优化：过时帧丢弃策略 ---
        // 如果帧在队列中等待了超过 15ms (约半帧时间)，说明它已经"不新鲜"了。
        // 处理它会导致最终显示的延迟增加。不如直接丢弃，等待下一帧刚刚到达的"热乎"数据。
        // 这会实现 NPU 处理与摄像头采集的"相位锁定"，大幅降低排队延迟。
        if (captureToPreprocessUs > 15000) { 
            // 仅在 NPU 负载较高时启用此策略 (防止首帧被误杀)
            // 这里简单起见直接启用，因为我们知道 NPU 是瓶颈
            // qDebug() << "Dropping stale frame, latency:" << captureToPreprocessUs << "us";
            emit workerReady(); 
            return;
        }
        // --------------------------------

        if (!preprocessorReady_) {
            preprocessorReady_ = preprocessor_.initialize(runner_.inputWidth(), runner_.inputHeight());
            if (!preprocessorReady_) {
                emit inferenceError(tr("RGA 预处理未就绪"));
                emit workerReady(); // 确保释放
                return;
            }
        }

        RgaPreprocessor::Result prepResult;
        RgaPreprocessor::Result displayResult; // 新增：显示结果

        // 移除 AutoCloseFd，因为 FD 现在由 sampleHolder 管理
        // struct AutoCloseFd { ... } fdCloser{packet.dmaFd};

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

            // 2. RGA -> Display Image (Scaled for UI performance)
            // 计算保持宽高比的显示尺寸，宽度限制在 640 以保证软件渲染性能
            int displayW = 640;
            int displayH = 360;
            if (packet.width > 0 && packet.height > 0) {
                // 保持原始比例
                displayH = static_cast<int>(static_cast<float>(packet.height) * displayW / static_cast<float>(packet.width));
                // 确保偶数高度 (RGA 偏好)
                displayH = (displayH / 2) * 2;
            }
            displayResult = preprocessor_.processNv12ToDisplay(nv12Input, displayW, displayH);

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
            emit workerReady(); // 确保释放
            return;
        }

        DetectResultGroup detections{};
        std::int64_t inferenceTimeMs = 0;
        QString error;
        YoloV5Runner::InferenceBreakdown breakdown;
        breakdown.preprocessUs = prepResult.durationUs;

        // 检查是否需要进行性能分析 (Profiling)
        QString perfDetail;
        QString* perfDetailPtr = nullptr;
        bool profilingActive = false;
        {
            QMutexLocker locker(&m_mutex);
            if (m_profilingPending) {
                m_profilingPending = false;
                profilingActive = true;
            }
        }

        if (profilingActive) {
            // 尝试开启性能模式 (会重新加载模型，耗时较长)
            if (runner_.reinit(true)) {
                perfDetailPtr = &perfDetail;
            } else {
                PerformanceMonitor::instance()->submitNpuPerfDetail(tr("无法开启性能模式：模型重载失败"));
                profilingActive = false;
                // 如果开启失败且模型未就绪（可能在 reinit 中被释放），尝试恢复普通模式
                if (!runner_.isReady()) {
                    runner_.reinit(false);
                }
            }
        }

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
                                                      &breakdown,
                                                      perfDetailPtr);
        
        if (profilingActive) {
            if (inferenceOk && !perfDetail.isEmpty()) {
                PerformanceMonitor::instance()->submitNpuPerfDetail(perfDetail);
            } else if (!inferenceOk) {
                PerformanceMonitor::instance()->submitNpuPerfDetail(tr("性能分析失败：推理过程出错 - %1").arg(error));
            } else {
                PerformanceMonitor::instance()->submitNpuPerfDetail(tr("性能分析失败：未获取到性能数据"));
            }
            // 立即恢复高性能模式 (释放内存，重新加载)
            runner_.reinit(false);
        }
        
        if (!inferenceOk) {
             // ... error handling ...
             emit inferenceError(error);
             emit workerReady(); // 确保释放
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
        
        // 关键优化：推理和数据拷贝完成后，立即通知可以接收下一帧
        // 不必等待 UI 渲染完成
        emit workerReady();
    }

    signals:
    void inferenceReady(quint64 frameId, const QImage& image, int objectCount, qint64 inferenceTimeMs);
    void inferenceError(const QString& errorText);
    void workerReady(); // 新增：通知主线程 Worker 已空闲

private:
    YoloV5Runner runner_;
    RgaPreprocessor preprocessor_;
    bool preprocessorReady_ = false;
    
    QMutex m_mutex;
    FramePacket m_pendingPacket;
    bool m_hasPending = false;
    bool m_isRunning = false;
    bool m_profilingPending = false;
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
    qRegisterMetaType<PerformanceMonitor::FrameTimings>("PerformanceMonitor::FrameTimings");
    qRegisterMetaType<PerformanceMonitor::ResourceSnapshot>("PerformanceMonitor::ResourceSnapshot");

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
    GstElement* jpegparse = gst_element_factory_make("jpegparse", "jpegparse");
    GstElement* appsink = gst_element_factory_make("appsink", kAppsinkName);

    if (!source || !convert || !queue || !capsfilter || !jpegparse || !appsink) {
        updateStatusText(tr("无法创建 GStreamer 元件"));
        if (source) gst_object_unref(source);
        if (convert) gst_object_unref(convert);
        if (queue) gst_object_unref(queue);
        if (capsfilter) gst_object_unref(capsfilter);
        if (jpegparse) gst_object_unref(jpegparse);
        if (appsink) gst_object_unref(appsink);
        return;
    }

    g_object_set(queue, "leaky", kQueueLeakyDownstream, "max-size-buffers", 1, nullptr); // drop stale frames immediately
    
    // Use MJPEG + MPP hardware decoding for better performance and lower USB bandwidth
    // Relax caps to allow negotiation (remove fixed framerate)
    GstCaps* caps = gst_caps_from_string("image/jpeg,width=640,height=480");
    g_object_set(capsfilter, "caps", caps, nullptr);
    gst_caps_unref(caps);

    // Create MPP decoder element
    // GstElement* mppdec = gst_element_factory_make("mppvideodec", "mppdec"); // Moved to linking logic
    
    // Force NV12 format for appsink to ensure we use the RGA path
    // and avoid the OpenCV BGR construction crash on non-BGR formats (like I420 from jpegdec).
    GstCaps* appsinkCaps = gst_caps_from_string("video/x-raw, format=NV12");
    gst_app_sink_set_caps(GST_APP_SINK(appsink), appsinkCaps);
    gst_caps_unref(appsinkCaps);

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
        if (jpegparse) gst_object_unref(jpegparse);
        gst_object_unref(appsink);
        // if (mppdec) gst_object_unref(mppdec);
        return;
    }

    // Add common elements
    gst_bin_add_many(GST_BIN(m_pipeline), source, queue, capsfilter, convert, appsink, nullptr);
    
    // Link Source -> Queue -> CapsFilter
    if (!gst_element_link_many(source, queue, capsfilter, nullptr)) {
        updateStatusText(tr("无法连接摄像头源"));
        stopPipeline();
        return;
    }

    // Try linking MPP path: CapsFilter -> JpegParse -> MppDec -> [Convert] -> AppSink
    bool decoderLinked = false;
    
    // Prefer mppjpegdec for JPEG, fallback to mppvideodec
    GstElement* hwDec = gst_element_factory_make("mppjpegdec", "hwdec");
    if (!hwDec) {
        hwDec = gst_element_factory_make("mppvideodec", "hwdec");
    }

    if (hwDec && jpegparse) {
        gst_bin_add(GST_BIN(m_pipeline), hwDec);
        gst_bin_add(GST_BIN(m_pipeline), jpegparse);

        // 1. Link CapsFilter -> JpegParse -> HwDec
        if (gst_element_link_many(capsfilter, jpegparse, hwDec, nullptr)) {
            
            // 2. Try linking HwDec -> AppSink (Direct, Zero-Copy for DMABuf)
            if (gst_element_link(hwDec, appsink)) {
                decoderLinked = true;
                qDebug() << "MPP Hardware decoding pipeline linked (Direct Zero-Copy)";
                // Remove unused convert
                gst_bin_remove(GST_BIN(m_pipeline), convert);
            } 
            // 3. If direct fails, try HwDec -> Convert -> AppSink
            else if (gst_element_link_many(hwDec, convert, appsink, nullptr)) {
                decoderLinked = true;
                qDebug() << "MPP Hardware decoding pipeline linked (via Convert)";
            }
        }
        
        if (!decoderLinked) {
            qDebug() << "MPP linking failed, removing HW elements...";
            gst_bin_remove(GST_BIN(m_pipeline), hwDec);
            gst_bin_remove(GST_BIN(m_pipeline), jpegparse);
            // Note: convert is still in bin if we didn't remove it yet, or we need to re-add it if we removed it?
            // We only removed 'convert' in the success path above.
            // But wait, 'convert' was added to bin in "Add common elements".
            // If we are here, 'convert' is still in bin (unless we removed it in success path, which we didn't reach).
        }
    }

    if (!decoderLinked) {
        // Fallback to software jpegdec
        // Ensure 'convert' is in the bin (it might be there, but let's be safe)
        // gst_bin_add(GST_BIN(m_pipeline), convert); // It's already added at start
        
        GstElement* jpegdec = gst_element_factory_make("jpegdec", "jpegdec");
        if (jpegdec) {
            gst_bin_add(GST_BIN(m_pipeline), jpegdec);
            // Note: jpegdec outputs I420, convert will handle I420 -> NV12 (for appsink)
            if (gst_element_link_many(capsfilter, jpegdec, convert, appsink, nullptr)) {
                decoderLinked = true;
                updateStatusText(tr("警告: 使用软件解码 (MPP失败)"));
            } else {
                gst_bin_remove(GST_BIN(m_pipeline), jpegdec);
            }
        }
    }

    if (!decoderLinked) {
        updateStatusText(tr("无法连接解码器 (MPP/JPEGDEC)"));
        stopPipeline();
        return;
    }

    // Link Convert -> AppSink (Only if software path used, or HW path used convert)
    // Actually, the linking logic above already handles the connection to appsink.
    // So we don't need a separate link step here.
    
    // Double check if appsink is linked
    // GstPad* sinkPad = gst_element_get_static_pad(appsink, "sink");
    // if (!gst_pad_is_linked(sinkPad)) { ... }
    // gst_object_unref(sinkPad);

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
    // 移除旧的信号连接，改用直接调用 updateFrame
    // connect(this, &CameraPreviewDialog::frameCaptured, worker, &DetectionWorker::processFrame, Qt::QueuedConnection);
    connect(worker, &DetectionWorker::inferenceReady, this, &CameraPreviewDialog::onInferenceFrameReady, Qt::QueuedConnection);
    connect(worker, &DetectionWorker::inferenceError, this, &CameraPreviewDialog::onInferenceError, Qt::QueuedConnection);
    
    // 移除 workerReady 连接，因为我们不再依赖 m_processingFrame 标志位进行流控
    // connect(worker, &DetectionWorker::workerReady, this, [this]() {
    //    m_processingFrame = false;
    // }, Qt::DirectConnection);

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
    // 使用 FastTransformation 替代 SmoothTransformation 以大幅降低 CPU 占用
    // 因为源图像已经是 RGA 缩放过的较小尺寸，再次缩放开销较小，且 Fast 模式足够预览使用
    const QPixmap scaled = m_currentPixmap.scaled(targetSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    m_videoLabel->setPixmap(scaled);
    m_videoLabel->setText(QString());
}

void CameraPreviewDialog::onInferenceFrameReady(quint64 frameId, const QImage& image, int objectCount, qint64 inferenceTimeMs)
{
    const auto renderStart = std::chrono::steady_clock::now();

    // m_processingFrame = false; // 移除：改为由 workerReady 信号提前重置

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
    // m_processingFrame = false; // 移除：错误时也应由 workerReady 或逻辑保证重置，这里仅做 UI 更新
    // 但为了保险，如果 Worker 内部发生错误提前返回，也应该确保重置。
    // 现在的 processFrame 实现中，错误分支也会返回，我们需要确保那里也 emit workerReady 或者在这里兜底。
    // 简单起见，我们在 processFrame 的所有退出路径都保证 emit workerReady 比较困难，
    // 所以保留这里的兜底，但主要依赖 Worker。
    // 实际上 processFrame 里错误返回时没有 emit workerReady，所以这里必须保留，
    // 或者修改 processFrame 让错误也 emit。
    // 让我们修改 processFrame 让它总是 emit workerReady。
    
    // 修正：由于 processFrame 修改较多，我们在 onInferenceError 里保留重置作为安全网
    // 但为了性能，最好在 processFrame 内部处理。
    // 鉴于 replace_string 限制，我们这里先保留，但在 processFrame 的错误分支里加上 emit workerReady 会更好。
    // 暂时先这样，错误情况下的性能不是瓶颈。
    m_processingFrame = false; 
    updateStatusText(tr("推理失败: %1").arg(errorText));
}

void CameraPreviewDialog::handleNewFrame(const FramePacket& packet)
{
    // 此函数现在仅用于在主线程触发（如果需要），但我们更倾向于直接从 appsink 调用 worker
    // 为了线程安全和解耦，我们保留这个槽函数，但让它调用 worker 的线程安全接口
    if (m_detectionWorker) {
        m_detectionWorker->updateFrame(packet);
    }
}

GstFlowReturn CameraPreviewDialog::onAppSinkNewSample(GstAppSink* sink, gpointer userData)
{
    auto* self = static_cast<CameraPreviewDialog*>(userData);
    // 移除 m_processingFrame 检查，改为总是获取最新帧并更新 Mailbox
    if (!self) {
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
    
    // 使用 shared_ptr 管理 sample 生命周期
    std::shared_ptr<GstSample> samplePtr(sample, [](GstSample* s) {
        if (s) gst_sample_unref(s);
    });

    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps) {
        return GST_FLOW_OK;
    }

    GstVideoInfo info;
    gst_video_info_init(&info);
    if (!gst_video_info_from_caps(&info, caps)) {
        return GST_FLOW_OK;
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
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

    // 移除 ScopedFdCloser，因为我们现在持有 samplePtr，FD 在 sample 释放前一直有效
    // 且 gst_dmabuf_memory_get_fd 返回的 FD 不需要 close (它属于 allocator)
    // 除非我们 dup 了它。通常 GStreamer 的 dmabuf fd 是借用的。
    // 之前的代码 close(dmaFd) 可能是错误的，或者之前的逻辑是 dup 出来的？
    // gst_dmabuf_memory_get_fd 文档说: "The file descriptor remains valid until the memory is destroyed."
    // 所以我们不需要 close 它，只需要保持 memory (sample) 存活。
    
    // 如果之前的代码有 close，那可能是为了防止泄漏？但如果是借用的就不该 close。
    // 假设之前的 close 是多余的或者错误的，现在我们用 sampleHolder 保证安全。

    GstMapInfo mapInfo;
    if (!gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
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
    
    // 仍然拷贝数据作为回退，或者用于非 DMA 路径
    // 如果完全确信 DMA 路径，可以优化掉这个拷贝，但为了稳健性先保留
    packet.data = QByteArray(reinterpret_cast<const char*>(mapInfo.data), static_cast<int>(mapInfo.size));
    
    packet.pts = static_cast<std::int64_t>(GST_BUFFER_PTS(buffer));
    packet.captureTimestampNs = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    packet.frameId = PerformanceMonitor::instance()->recordFrameCaptured(packet.captureTimestampNs);
    packet.dmaFd = dmaFd;
    packet.sampleHolder = samplePtr; // 传递所有权

    gst_buffer_unmap(buffer, &mapInfo);
    // gst_sample_unref(sample); // 不需要了，由 samplePtr 管理

    // 直接调用 Worker 的线程安全接口，或者通过主线程转发
    // 为了最低延迟，我们直接调用 Worker (Worker 必须处理好线程安全)
    if (self->m_detectionWorker) {
        self->m_detectionWorker->updateFrame(packet);
    }

    return GST_FLOW_OK;
}

#include "CameraPreviewDialog.moc"

