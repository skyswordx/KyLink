#ifndef CAMERAPREVIEWDIALOG_H
#define CAMERAPREVIEWDIALOG_H

#include <QDialog>
#include <QCamera>
#include <QList>
#include <QCameraInfo>
#include <memory>

class QLabel;
class QComboBox;
class QPushButton;
class QCameraViewfinder;

class CameraPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraPreviewDialog(QWidget* parent = nullptr);
    ~CameraPreviewDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDeviceChanged(int index);
    void onStartStopClicked();
    void handleCameraStateChanged(QCamera::State state);
    void handleCameraError(QCamera::Error error);

private:
    void initializeUi();
    void populateDevices();
    void createCameraForIndex(int index);
    void releaseCamera();
    void updateControls();
    void updateStatusText(const QString& text);

    QComboBox* m_deviceCombo;
    QPushButton* m_startStopButton;
    QLabel* m_statusLabel;
    QCameraViewfinder* m_viewfinder;

    QList<QCameraInfo> m_availableDevices;
    std::unique_ptr<QCamera> m_camera;
};

#endif // CAMERAPREVIEWDIALOG_H
