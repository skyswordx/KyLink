#ifndef VIDEOPLAYERDIALOG_H
#define VIDEOPLAYERDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QImage>
#include <QPixmap>
#include <cstdint>

class VideoReceiver;

/**
 * @brief 视频流接收显示窗口
 * 
 * 运行在 Ubuntu 端，接收并显示来自 RK3566 的视频流。
 */
class VideoPlayerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit VideoPlayerDialog(QWidget* parent = nullptr);
    ~VideoPlayerDialog() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onStartStopClicked();
    void onFrameReceived(const QImage& image, uint32_t frameId, const QString& senderIp);
    void onListeningStarted(uint16_t port);
    void onListeningStopped();
    void onReceiveError(const QString& error);
    void onStatsUpdated(int fps, int lostFrames);

private:
    void initializeUi();
    void updateDisplayedPixmap();

    QLabel* m_videoLabel;
    QLabel* m_statusLabel;
    QLabel* m_statsLabel;
    QPushButton* m_startStopButton;
    
    VideoReceiver* m_receiver;
    bool m_isListening;
    QPixmap m_currentPixmap;
    QString m_currentSender;
};

#endif // VIDEOPLAYERDIALOG_H
