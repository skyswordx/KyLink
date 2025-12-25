#ifndef CAMERAPREVIEWDIALOG_H
#define CAMERAPREVIEWDIALOG_H

#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qdialog.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <atomic>
#include <cstdint>
#include <memory> // for std::shared_ptr
#include <vector>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstDevice GstDevice;
typedef struct _GstAppSink GstAppSink;

class QLabel;
class QComboBox;
class QPushButton;
class QLineEdit;
class QTimer;
class QCloseEvent;
class QResizeEvent;
class QThread;
class VideoStreamer;

struct FramePacket {
    quint64 frameId = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
    QByteArray data;
    std::int64_t pts = 0;
    qint64 captureTimestampNs = 0;
    int dmaFd = -1;
    GstVideoFormat videoFormat = GST_VIDEO_FORMAT_UNKNOWN;
    int yStride = 0;
    int uvStride = 0;
    int yPlaneOffset = 0;
    int uvPlaneOffset = 0;

    // 持有 GstSample 的引用，确保 dmaFd 在处理期间有效
    std::shared_ptr<void> sampleHolder;
};

Q_DECLARE_METATYPE(FramePacket)

class CameraPreviewDialog : public QDialog
{
    Q_OBJECT

    class DetectionWorker;

public:
    explicit CameraPreviewDialog(QWidget* parent = nullptr);
    ~CameraPreviewDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

signals:
    void frameCaptured(const FramePacket& packet);

private slots:
    void onDeviceChanged(int index);
    void onStartStopClicked();
    void processBusMessages();
    void onInferenceFrameReady(quint64 frameId, const QImage& image, int objectCount, qint64 inferenceTimeMs);
    void onInferenceError(const QString& errorText);
    
    // 视频推流槽
    void onStreamClicked();
    void onStreamingStarted(const QString& targetIp);
    void onStreamingStopped();
    void onFrameSent(uint32_t frameId, int chunks);
    void onStreamError(const QString& error);

private:
    void initializeUi();
    void populateDevices();
    void clearDevices();
    void startPipelineForIndex(int index);
    void stopPipeline();
    void updateControls();
    void updateStatusText(const QString& text);
    bool isPipelineActive() const;
    static void ensureGStreamerInitialized();
    bool ensureDetectionWorker(QString* errorOut);
    void disposeDetectionWorker();
    void clearVideoLabel();
    void updateDisplayedPixmap();
    void handleNewFrame(const FramePacket& packet);
    static GstFlowReturn onAppSinkNewSample(GstAppSink* sink, gpointer userData);
    
    // 发送当前帧到视频流
    void sendFrameToStream(const QImage& image);

    struct VideoDevice {
        QString label;
        GstDevice* handle = nullptr;
    };

    QComboBox* m_deviceCombo;
    QPushButton* m_startStopButton;
    QLabel* m_statusLabel;
    QLabel* m_videoLabel;
    QTimer* m_busPollTimer;

    std::vector<VideoDevice> m_devices;

    GstElement* m_pipeline;
    GstElement* m_appSink;
    GstBus* m_bus;
    bool m_isPlaying;
    std::atomic<bool> m_processingFrame;
    QThread* m_detectionThread;
    DetectionWorker* m_detectionWorker;
    QPixmap m_currentPixmap;
    
    // 视频推流
    QLineEdit* m_streamTargetEdit;
    QPushButton* m_streamButton;
    QLabel* m_streamStatusLabel;
    VideoStreamer* m_videoStreamer;
    bool m_isStreaming;
};

#endif // CAMERAPREVIEWDIALOG_H

