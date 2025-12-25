#ifndef VIDEOSTREAMER_H
#define VIDEOSTREAMER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QMutex>
#include <atomic>
#include <cstdint>
#include <vector>

/**
 * @brief 视频流发送端
 * 
 * 负责将JPEG帧通过UDP分包发送到指定接收端。
 * 运行在RK3566端，从CameraPreviewDialog获取处理后的帧。
 */
class VideoStreamer : public QObject
{
    Q_OBJECT

public:
    explicit VideoStreamer(QObject* parent = nullptr);
    ~VideoStreamer();

    /**
     * @brief 开始向目标发送视频流
     * @param targetIp 目标IP地址
     * @param targetPort 目标端口（默认VIDEO_PORT）
     * @return 成功返回true
     */
    bool startStreaming(const QString& targetIp, uint16_t targetPort = 2426);

    /**
     * @brief 停止发送
     */
    void stopStreaming();

    /**
     * @brief 是否正在发送
     */
    bool isStreaming() const { return m_streaming; }

    /**
     * @brief 发送一帧JPEG数据
     * @param jpegData JPEG编码数据
     * @param width 帧宽度
     * @param height 帧高度
     * @return 发送成功返回true
     */
    bool sendFrame(const std::vector<uint8_t>& jpegData, uint16_t width, uint16_t height);

    /**
     * @brief 获取当前目标IP
     */
    QString targetIp() const { return m_targetIp; }

signals:
    void streamingStarted(const QString& targetIp);
    void streamingStopped();
    void frameSent(uint32_t frameId, int chunks);
    void sendError(const QString& error);

private:
    QUdpSocket* m_socket = nullptr;
    QHostAddress m_targetAddress;
    uint16_t m_targetPort = 2426;
    QString m_targetIp;
    std::atomic<bool> m_streaming{false};
    std::atomic<uint32_t> m_frameId{0};
    QMutex m_mutex;
};

#endif // VIDEOSTREAMER_H
