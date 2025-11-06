#ifndef FEIQBACKEND_H
#define FEIQBACKEND_H

#include <QObject>
#include <QStringList>
#include <memory>
#include <QHash>
#include <QByteArray>

#include "domain/FeiqTypes.h"
#include "feiqengine.h" // 源自 feiq/feiqlib (Mac 飞秋) 项目

class FeiqBackend : public QObject, public IFeiqView
{
    Q_OBJECT

public:
    explicit FeiqBackend(QObject* parent = nullptr);
    ~FeiqBackend() override;

    void setIdentity(const QString& username, const QString& hostname);
    void setBroadcastAddresses(const QStringList& addresses);

    bool start();
    void stop();
    bool isRunning() const { return m_running; }

    QList<FeiqFellowInfo> fellows() const;

    bool sendText(const QString& ip, const QString& text, const QString& format = QString(), QString* error = nullptr);
    bool sendFiles(const QString& ip, const QStringList& filePaths, QString* error = nullptr);
    bool acceptFile(const QString& ip, quint32 packetNo, quint32 fileId, const QString& savePath, QString* error = nullptr);
    void cancelFileTask(quint32 packetNo, quint32 fileId, bool upload);

    // Loopback test utilities
    void enableLoopbackTestUser(const QString& displayName = QString());
    void simulateTestUserIncomingText(const QString& text);
    void simulateTestUserIncomingFile(const QString& fileName,
                                      qint64 fileSize = -1,
                                      const QByteArray& data = QByteArray());

signals:
    void engineStarted();
    void engineError(const QString& error);
    void fellowUpdated(const FeiqFellowInfo& fellow);
    void messageReceived(const FeiqMessage& message);
    void sendTimeout(const FeiqFellowInfo& fellow, const QString& description);
    void fileTaskUpdated(const FeiqFileTaskInfo& info);

protected:
    void onEvent(std::shared_ptr<ViewEvent> event) override;
    void onStateChanged(FileTask* fileTask) override;
    void onProgress(FileTask* fileTask) override;

private:
    std::shared_ptr<Fellow> ensureFellow(const QString& ip);
    FeiqFellowInfo toFellowInfo(const std::shared_ptr<Fellow>& fellow) const;
    FeiqMessageContent toMessageContent(const std::shared_ptr<Content>& content) const;
    FeiqFileOffer toFileOffer(const std::shared_ptr<FileContent>& file) const;
    FeiqFileTaskInfo toFileTaskInfo(FileTask* task) const;
    FeiqFileTaskState toFileTaskState(FileTaskState state) const;

    void handleFellowEvent(const std::shared_ptr<FellowViewEvent>& event);
    void handleMessageEvent(const std::shared_ptr<MessageViewEvent>& event);
    void handleSendTimeoutEvent(const std::shared_ptr<SendTimeoEvent>& event);

    std::shared_ptr<FileTask> findFileTask(quint32 packetNo, quint32 fileId, FileTaskType type) const;

    bool isTestUser(const QString& ip) const;
    void emitTestUserMessage(const QString& text);
    void emitTestUserFileOffer(const FeiqFileOffer& offer);
    bool handleTestUserSendText(const QString& text, QString* error);
    bool handleTestUserSendFiles(const QStringList& filePaths, QString* error);
    bool handleTestUserAcceptFile(quint32 packetNo, quint32 fileId, const QString& savePath, QString* error);

private:
    FeiqEngine m_engine;
    QString m_username;
    QString m_hostname;
    QStringList m_broadcastAddresses;
    bool m_running = false;

    bool m_testUserEnabled = false;
    FeiqFellowInfo m_testUser;
    quint32 m_testPacketCounter = 1;
    quint32 m_testFileIdCounter = 1;
    QHash<quint64, QByteArray> m_testIncomingFiles;
};

#endif // FEIQBACKEND_H

