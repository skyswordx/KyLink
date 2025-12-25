#ifndef VIDEORECEIVER_H
#define VIDEORECEIVER_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QMutex>
#include <QImage>
#include <map>
#include <vector>
#include <cstdint>

/**
 * @brief 视频流接收端
 * 
 * 负责接收UDP分包、重组帧、解码JPEG并显示。
 * 运行在Ubuntu端。
 */
class VideoReceiver : public QObject
{
    Q_OBJECT

public:
    explicit VideoReceiver(QObject* parent = nullptr);
    ~VideoReceiver();

    /**
     * @brief 开始监听视频流
     * @param port 监听端口（默认VIDEO_PORT）
     * @return 成功返回true
     */
    bool startListening(uint16_t port = 2426);

    /**
     * @brief 停止监听
     */
    void stopListening();

    /**
     * @brief 是否正在监听
     */
    bool isListening() const { return m_listening; }

signals:
    /**
     * @brief 收到完整帧信号
     * @param image 解码后的QImage
     * @param frameId 帧ID
     * @param senderIp 发送方IP
     */
    void frameReceived(const QImage& image, uint32_t frameId, const QString& senderIp);

    /**
     * @brief 监听开始
     */
    void listeningStarted(uint16_t port);

    /**
     * @brief 监听停止
     */
    void listeningStopped();

    /**
     * @brief 接收错误
     */
    void receiveError(const QString& error);

    /**
     * @brief 统计信息
     */
    void statsUpdated(int fps, int lostFrames);

private slots:
    void onReadyRead();
    void onTimeoutCheck();

private:
    struct FrameBuffer {
        uint32_t frameId = 0;
        uint16_t totalChunks = 0;
        std::vector<std::vector<uint8_t>> chunks;
        std::vector<bool> received;
        qint64 firstChunkTime = 0;
        QString senderIp;

        bool isComplete() const;
        std::vector<uint8_t> assemble() const;
    };

    void processChunk(const QHostAddress& sender, const QByteArray& data);
    void completeFrame(uint32_t frameId);
    void cleanupOldFrames();

    QUdpSocket* m_socket = nullptr;
    QTimer* m_timeoutTimer = nullptr;
    bool m_listening = false;
    QMutex m_mutex;

    std::map<uint32_t, FrameBuffer> m_frameBuffers;
    uint32_t m_lastCompletedFrame = 0;
    int m_framesReceived = 0;
    int m_framesLost = 0;
    qint64 m_lastStatsTime = 0;
};

#endif // VIDEORECEIVER_H
