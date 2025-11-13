#ifndef CAMERAPREVIEWDIALOG_H
#define CAMERAPREVIEWDIALOG_H

#include <QDialog>
#include <QString>
#include <vector>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;
typedef struct _GstDevice GstDevice;

class QLabel;
class QComboBox;
class QPushButton;
class QWidget;
class QTimer;
class QCloseEvent;
class QResizeEvent;

class CameraPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraPreviewDialog(QWidget* parent = nullptr);
    ~CameraPreviewDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onDeviceChanged(int index);
    void onStartStopClicked();
    void processBusMessages();

private:
    void initializeUi();
    void populateDevices();
    void clearDevices();
    void startPipelineForIndex(int index);
    void stopPipeline();
    void updateControls();
    void updateStatusText(const QString& text);
    void bindOverlayWindow();
    bool isPipelineActive() const;
    static void ensureGStreamerInitialized();

    struct VideoDevice {
        QString label;
        GstDevice* handle = nullptr;
    };

    QComboBox* m_deviceCombo;
    QPushButton* m_startStopButton;
    QLabel* m_statusLabel;
    QWidget* m_videoWidget;
    QTimer* m_busPollTimer;

    std::vector<VideoDevice> m_devices;

    GstElement* m_pipeline;
    GstElement* m_videoSink;
    GstBus* m_bus;
    bool m_isPlaying;
};

#endif // CAMERAPREVIEWDIALOG_H
