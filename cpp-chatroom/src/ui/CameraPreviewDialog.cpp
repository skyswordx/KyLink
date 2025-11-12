#include "ui/CameraPreviewDialog.h"

#include <QtGlobal>
#include <QCameraViewfinder>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QCloseEvent>

CameraPreviewDialog::CameraPreviewDialog(QWidget* parent)
    : QDialog(parent)
    , m_deviceCombo(nullptr)
    , m_startStopButton(nullptr)
    , m_statusLabel(nullptr)
    , m_viewfinder(nullptr)
{
    setWindowTitle(tr("摄像头预览"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(720, 520);

    initializeUi();
    populateDevices();
}

CameraPreviewDialog::~CameraPreviewDialog()
{
    releaseCamera();
}

void CameraPreviewDialog::closeEvent(QCloseEvent* event)
{
    releaseCamera();
    event->accept();
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

    m_viewfinder = new QCameraViewfinder(this);
    m_viewfinder->setMinimumHeight(360);

    m_statusLabel = new QLabel(tr("正在初始化摄像头列表..."), this);

    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(m_viewfinder, 1);
    mainLayout->addWidget(m_statusLabel);

    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraPreviewDialog::onDeviceChanged);
    connect(m_startStopButton, &QPushButton::clicked,
            this, &CameraPreviewDialog::onStartStopClicked);
}

void CameraPreviewDialog::populateDevices()
{
    m_availableDevices = QCameraInfo::availableCameras();
    m_deviceCombo->clear();

    for (const auto& info : m_availableDevices) {
        m_deviceCombo->addItem(info.description());
    }

    const bool hasDevices = !m_availableDevices.isEmpty();
    m_deviceCombo->setEnabled(hasDevices);
    m_startStopButton->setEnabled(hasDevices);

    if (hasDevices) {
        m_deviceCombo->setCurrentIndex(0);
        createCameraForIndex(0);
    } else {
        updateStatusText(tr("未检测到可用的摄像头"));
    }
}

void CameraPreviewDialog::onDeviceChanged(int index)
{
    createCameraForIndex(index);
}

void CameraPreviewDialog::onStartStopClicked()
{
    if (!m_camera) {
        createCameraForIndex(m_deviceCombo->currentIndex());
        return;
    }

    if (m_camera->state() == QCamera::ActiveState) {
        m_camera->stop();
        updateStatusText(tr("摄像头已停止"));
    } else {
        m_camera->start();
        updateStatusText(tr("正在打开摄像头..."));
    }

    updateControls();
}

void CameraPreviewDialog::handleCameraStateChanged(QCamera::State state)
{
    switch (state) {
    case QCamera::ActiveState:
        updateStatusText(tr("预览中"));
        break;
    case QCamera::LoadedState:
        updateStatusText(tr("摄像头已加载"));
        break;
    case QCamera::UnloadedState:
        updateStatusText(tr("摄像头未启动"));
        break;
    }

    updateControls();
}

void CameraPreviewDialog::handleCameraError(QCamera::Error error)
{
    if (error == QCamera::NoError) {
        return;
    }

    const QString errorText = m_camera ? m_camera->errorString() : tr("未知错误");
    updateStatusText(tr("摄像头错误: %1").arg(errorText));
    updateControls();
}

void CameraPreviewDialog::createCameraForIndex(int index)
{
    releaseCamera();

    if (index < 0 || index >= m_availableDevices.size()) {
        updateStatusText(tr("未检测到可用的摄像头"));
        updateControls();
        return;
    }

    m_camera = std::make_unique<QCamera>(m_availableDevices.at(index));
    m_camera->setViewfinder(m_viewfinder);

    connect(m_camera.get(), &QCamera::stateChanged,
            this, &CameraPreviewDialog::handleCameraStateChanged);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_camera.get(), &QCamera::errorOccurred,
            this, &CameraPreviewDialog::handleCameraError);
#else
    connect(m_camera.get(), QOverload<QCamera::Error>::of(&QCamera::error),
            this, &CameraPreviewDialog::handleCameraError);
#endif

    m_camera->start();
    updateStatusText(tr("正在打开摄像头..."));
    updateControls();
}

void CameraPreviewDialog::releaseCamera()
{
    if (!m_camera) {
        return;
    }

    disconnect(m_camera.get(), nullptr, this, nullptr);
    if (m_camera->state() == QCamera::ActiveState) {
        m_camera->stop();
    }
    m_camera.reset();
}

void CameraPreviewDialog::updateControls()
{
    if (!m_camera) {
        m_startStopButton->setText(tr("开始预览"));
        return;
    }

    const bool isActive = m_camera->state() == QCamera::ActiveState;
    m_startStopButton->setText(isActive ? tr("停止预览") : tr("开始预览"));
}

void CameraPreviewDialog::updateStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}
