#ifndef CAMERAPREVIEWDIALOG_H
#define CAMERAPREVIEWDIALOG_H

#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qdialog.h>
#include <gst/gst.h>
#include <atomic>
#include <cstdint>
#include <vector>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstDevice GstDevice;
typedef struct _GstAppSink GstAppSink;

class QLabel;
class QComboBox;
class QPushButton;
class QTimer;
class QCloseEvent;
class QResizeEvent;
class QThread;

struct FramePacket {
    int width = 0;
    int height = 0;
    int stride = 0;
    QByteArray data;
    std::int64_t pts = 0;
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
    void onInferenceFrameReady(const QImage& image, int objectCount, qint64 inferenceTimeMs);
    void onInferenceError(const QString& errorText);

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
};

#endif // CAMERAPREVIEWDIALOG_H
