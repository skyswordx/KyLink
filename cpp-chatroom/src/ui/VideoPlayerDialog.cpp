#include "ui/VideoPlayerDialog.h"
#include "video/VideoReceiver.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QDebug>

VideoPlayerDialog::VideoPlayerDialog(QWidget* parent)
    : QDialog(parent)
    , m_videoLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_statsLabel(nullptr)
    , m_startStopButton(nullptr)
    , m_receiver(nullptr)
    , m_isListening(false)
{
    setWindowTitle(tr("视频流接收"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(720, 540);

    initializeUi();
}

VideoPlayerDialog::~VideoPlayerDialog()
{
    if (m_receiver) {
        m_receiver->stopListening();
        delete m_receiver;
        m_receiver = nullptr;
    }
}

void VideoPlayerDialog::closeEvent(QCloseEvent* event)
{
    if (m_receiver) {
        m_receiver->stopListening();
    }
    event->accept();
}

void VideoPlayerDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateDisplayedPixmap();
}

void VideoPlayerDialog::initializeUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // 视频显示区域
    m_videoLabel = new QLabel(this);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumHeight(360);
    m_videoLabel->setStyleSheet("background-color: #202020; color: #ffffff;");
    m_videoLabel->setText(tr("等待视频流..."));

    // 控制区域
    auto* controlLayout = new QHBoxLayout();
    m_startStopButton = new QPushButton(tr("开始接收"), this);
    m_statsLabel = new QLabel(tr("FPS: -- | 丢帧: --"), this);
    
    controlLayout->addWidget(m_startStopButton);
    controlLayout->addStretch();
    controlLayout->addWidget(m_statsLabel);

    // 状态栏
    m_statusLabel = new QLabel(tr("未监听"), this);

    mainLayout->addWidget(m_videoLabel, 1);
    mainLayout->addLayout(controlLayout);
    mainLayout->addWidget(m_statusLabel);

    connect(m_startStopButton, &QPushButton::clicked,
            this, &VideoPlayerDialog::onStartStopClicked);
}

void VideoPlayerDialog::onStartStopClicked()
{
    if (m_isListening) {
        // 停止监听
        if (m_receiver) {
            m_receiver->stopListening();
        }
    } else {
        // 开始监听
        if (!m_receiver) {
            m_receiver = new VideoReceiver(this);
            connect(m_receiver, &VideoReceiver::frameReceived,
                    this, &VideoPlayerDialog::onFrameReceived);
            connect(m_receiver, &VideoReceiver::listeningStarted,
                    this, &VideoPlayerDialog::onListeningStarted);
            connect(m_receiver, &VideoReceiver::listeningStopped,
                    this, &VideoPlayerDialog::onListeningStopped);
            connect(m_receiver, &VideoReceiver::receiveError,
                    this, &VideoPlayerDialog::onReceiveError);
            connect(m_receiver, &VideoReceiver::statsUpdated,
                    this, &VideoPlayerDialog::onStatsUpdated);
        }

        if (!m_receiver->startListening()) {
            m_statusLabel->setText(tr("无法开始监听"));
            return;
        }
    }
}

void VideoPlayerDialog::onFrameReceived(const QImage& image, uint32_t frameId, const QString& senderIp)
{
    Q_UNUSED(frameId);

    if (image.isNull()) {
        return;
    }

    m_currentSender = senderIp;
    m_currentPixmap = QPixmap::fromImage(image);
    updateDisplayedPixmap();

    if (m_videoLabel) {
        m_videoLabel->setText(QString());
    }
}

void VideoPlayerDialog::onListeningStarted(uint16_t port)
{
    m_isListening = true;
    m_startStopButton->setText(tr("停止接收"));
    m_statusLabel->setText(tr("监听端口 %1，等待视频流...").arg(port));
    m_statusLabel->setStyleSheet("color: green;");
}

void VideoPlayerDialog::onListeningStopped()
{
    m_isListening = false;
    m_startStopButton->setText(tr("开始接收"));
    m_statusLabel->setText(tr("已停止监听"));
    m_statusLabel->setStyleSheet("");
    m_videoLabel->setText(tr("等待视频流..."));
    m_currentPixmap = QPixmap();
}

void VideoPlayerDialog::onReceiveError(const QString& error)
{
    m_statusLabel->setText(tr("错误: %1").arg(error));
    m_statusLabel->setStyleSheet("color: red;");
}

void VideoPlayerDialog::onStatsUpdated(int fps, int lostFrames)
{
    m_statsLabel->setText(tr("FPS: %1 | 丢帧: %2").arg(fps).arg(lostFrames));
    
    if (!m_currentSender.isEmpty()) {
        m_statusLabel->setText(tr("接收来自 %1 的视频流").arg(m_currentSender));
    }
}

void VideoPlayerDialog::updateDisplayedPixmap()
{
    if (m_currentPixmap.isNull() || !m_videoLabel) {
        return;
    }

    QSize labelSize = m_videoLabel->size();
    if (labelSize.isEmpty()) {
        return;
    }

    QPixmap scaled = m_currentPixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_videoLabel->setPixmap(scaled);
}
