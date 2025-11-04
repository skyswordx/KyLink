#include "FeiqBackend.h"

#include <QMetaObject>
#include <QMetaType>
#include <QFileInfo>
#include <QTimer>
#include <QFile>

#include "feiqmodel.h"
#include "content.h"
#include "filetask.h"

#include <list>
#include <chrono>

namespace {

template <typename T>
void registerMeta(const char* name)
{
    qRegisterMetaType<T>(name);
}

} // namespace

FeiqBackend::FeiqBackend(QObject* parent)
    : QObject(parent)
{
    registerMeta<FeiqFellowInfo>("FeiqFellowInfo");
    registerMeta<FeiqFileOffer>("FeiqFileOffer");
    registerMeta<FeiqMessage>("FeiqMessage");
    registerMeta<FeiqMessageContent>("FeiqMessageContent");
    registerMeta<FeiqFileTaskInfo>("FeiqFileTaskInfo");
    registerMeta<FeiqContentType>("FeiqContentType");
    registerMeta<FeiqFileTaskState>("FeiqFileTaskState");

    m_engine.setView(this);
}

FeiqBackend::~FeiqBackend()
{
    stop();
}

void FeiqBackend::setIdentity(const QString& username, const QString& hostname)
{
    m_username = username;
    m_hostname = hostname;
    m_engine.setMyName(username.toStdString());
    m_engine.setMyHost(hostname.toStdString());
}

void FeiqBackend::setBroadcastAddresses(const QStringList& addresses)
{
    m_broadcastAddresses = addresses;
    for (const auto& ip : addresses) {
        m_engine.addToBroadcast(ip.toStdString());
    }
}

bool FeiqBackend::start()
{
    if (m_running) {
        return true;
    }

    auto result = m_engine.start();
    if (!result.first) {
        emit engineError(QString::fromStdString(result.second));
        return false;
    }

    m_running = true;
    emit engineStarted();
    return true;
}

void FeiqBackend::stop()
{
    if (!m_running) {
        return;
    }
    m_engine.stop();
    m_running = false;
}

QList<FeiqFellowInfo> FeiqBackend::fellows() const
{
    QList<FeiqFellowInfo> list;
    auto fellows = m_engine.getModel().searchFellow("");
    for (const auto& f : fellows) {
        list.append(toFellowInfo(f));
    }
    return list;
}

bool FeiqBackend::sendText(const QString& ip, const QString& text, const QString& format, QString* error)
{
    if (text.trimmed().isEmpty()) {
        if (error) {
            *error = tr("消息内容不能为空");
        }
        return false;
    }

    if (isTestUser(ip)) {
        return handleTestUserSendText(text, error);
    }

    auto fellow = ensureFellow(ip);

    auto content = std::make_shared<TextContent>();
    content->text = text.toStdString();
    content->format = format.toStdString();

    auto result = m_engine.send(fellow, content);
    if (!result.first) {
        if (error) {
            *error = QString::fromStdString(result.second);
        }
        return false;
    }
    return true;
}

bool FeiqBackend::sendFiles(const QString& ip, const QStringList& filePaths, QString* error)
{
    if (filePaths.isEmpty()) {
        return true;
    }

    if (isTestUser(ip)) {
        return handleTestUserSendFiles(filePaths, error);
    }

    auto fellow = ensureFellow(ip);
    std::list<std::shared_ptr<FileContent>> files;
    for (const auto& path : filePaths) {
        QFileInfo info(path);
        if (!info.exists()) {
            if (error) {
                *error = tr("文件不存在: %1").arg(path);
            }
            return false;
        }

        auto content = FileContent::createFileContentToSend(path.toStdString());
        if (!content) {
            if (error) {
                *error = tr("无法读取文件: %1").arg(path);
            }
            return false;
        }

        files.push_back(std::shared_ptr<FileContent>(content.release()));
    }

    auto result = m_engine.sendFiles(fellow, files);
    if (!result.first) {
        if (error) {
            *error = QString::fromStdString(result.second);
        }
        return false;
    }

    return true;
}

bool FeiqBackend::acceptFile(const QString& ip, quint32 packetNo, quint32 fileId, const QString& savePath, QString* error)
{
    if (isTestUser(ip)) {
        return handleTestUserAcceptFile(packetNo, fileId, savePath, error);
    }

    auto task = findFileTask(packetNo, fileId, FileTaskType::Download);
    if (!task) {
        if (error) {
            *error = tr("未找到对应的文件任务");
        }
        return false;
    }

    auto content = task->getContent();
    if (!content) {
        if (error) {
            *error = tr("文件任务无效");
        }
        return false;
    }

    content->path = savePath.toStdString();
    auto success = m_engine.downloadFile(task.get());
    if (!success && error) {
        *error = tr("启动文件下载失败");
    }
    return success;
}

void FeiqBackend::cancelFileTask(quint32 packetNo, quint32 fileId, bool upload)
{
    auto type = upload ? FileTaskType::Upload : FileTaskType::Download;
    auto task = findFileTask(packetNo, fileId, type);
    if (task) {
        task->cancel();
    }
}

void FeiqBackend::onEvent(std::shared_ptr<ViewEvent> event)
{
    switch (event->what) {
    case ViewEventType::FELLOW_UPDATE:
        handleFellowEvent(std::static_pointer_cast<FellowViewEvent>(event));
        break;
    case ViewEventType::MESSAGE:
        handleMessageEvent(std::static_pointer_cast<MessageViewEvent>(event));
        break;
    case ViewEventType::SEND_TIMEO:
        handleSendTimeoutEvent(std::static_pointer_cast<SendTimeoEvent>(event));
        break;
    default:
        break;
    }
}

void FeiqBackend::onStateChanged(FileTask* fileTask)
{
    auto info = toFileTaskInfo(fileTask);
    QMetaObject::invokeMethod(this, [this, info]() {
        emit fileTaskUpdated(info);
    }, Qt::QueuedConnection);
}

void FeiqBackend::onProgress(FileTask* fileTask)
{
    auto info = toFileTaskInfo(fileTask);
    QMetaObject::invokeMethod(this, [this, info]() {
        emit fileTaskUpdated(info);
    }, Qt::QueuedConnection);
}

std::shared_ptr<Fellow> FeiqBackend::ensureFellow(const QString& ip)
{
    auto fellow = m_engine.getModel().findFirstFellowOf(ip.toStdString());
    if (fellow) {
        return fellow;
    }

    auto created = std::make_shared<Fellow>();
    created->setIp(ip.toStdString());
    created->setOnLine(true);
    created->setName(ip.toStdString());
    return created;
}

FeiqFellowInfo FeiqBackend::toFellowInfo(const std::shared_ptr<Fellow>& fellow) const
{
    FeiqFellowInfo info;
    if (!fellow) {
        return info;
    }

    info.ip = QString::fromStdString(fellow->getIp());
    info.name = QString::fromStdString(fellow->getName());
    info.host = QString::fromStdString(fellow->getHost());
    info.mac = QString::fromStdString(fellow->getMac());
    info.online = fellow->isOnLine();
    return info;
}

FeiqMessageContent FeiqBackend::toMessageContent(const std::shared_ptr<Content>& content) const
{
    FeiqMessageContent result;
    if (!content) {
        return result;
    }

    switch (content->type()) {
    case ContentType::Text: {
        auto text = std::static_pointer_cast<TextContent>(content);
        result.type = FeiqContentType::Text;
        result.text = QString::fromStdString(text->text);
        result.format = QString::fromStdString(text->format);
        break;
    }
    case ContentType::Knock:
        result.type = FeiqContentType::Knock;
        break;
    case ContentType::File: {
        auto file = std::static_pointer_cast<FileContent>(content);
        result.type = FeiqContentType::File;
        result.file = toFileOffer(file);
        break;
    }
    case ContentType::Image: {
        auto image = std::static_pointer_cast<ImageContent>(content);
        result.type = FeiqContentType::Image;
        result.imageId = QString::fromStdString(image->id);
        break;
    }
    case ContentType::Id: {
        auto id = std::static_pointer_cast<IdContent>(content);
        result.type = FeiqContentType::Id;
        result.contentId = id->id;
        break;
    }
    }

    return result;
}

FeiqFileOffer FeiqBackend::toFileOffer(const std::shared_ptr<FileContent>& file) const
{
    FeiqFileOffer offer;
    if (!file) {
        return offer;
    }

    offer.packetNo = static_cast<quint32>(file->packetNo);
    offer.fileId = file->fileId;
    offer.fileName = QString::fromStdString(file->filename);
    offer.localPath = QString::fromStdString(file->path);
    offer.fileSize = file->size;
    offer.fileType = file->fileType;
    return offer;
}

FeiqFileTaskInfo FeiqBackend::toFileTaskInfo(FileTask* task) const
{
    FeiqFileTaskInfo info;
    if (!task) {
        return info;
    }

    info.upload = task->type() == FileTaskType::Upload;
    info.progress = task->getProcess();
    info.state = toFileTaskState(task->getState());
    info.detail = QString::fromStdString(task->getDetailInfo());
    info.fellow = toFellowInfo(task->fellow());
    info.file = toFileOffer(task->getContent());
    return info;
}

FeiqFileTaskState FeiqBackend::toFileTaskState(FileTaskState state) const
{
    switch (state) {
    case FileTaskState::NotStart:
        return FeiqFileTaskState::NotStart;
    case FileTaskState::Running:
        return FeiqFileTaskState::Running;
    case FileTaskState::Finish:
        return FeiqFileTaskState::Finished;
    case FileTaskState::Error:
        return FeiqFileTaskState::Error;
    case FileTaskState::Canceled:
        return FeiqFileTaskState::Canceled;
    }
    return FeiqFileTaskState::NotStart;
}

void FeiqBackend::handleFellowEvent(const std::shared_ptr<FellowViewEvent>& event)
{
    auto info = toFellowInfo(event->fellow);
    QMetaObject::invokeMethod(this, [this, info]() {
        emit fellowUpdated(info);
    }, Qt::QueuedConnection);
}

void FeiqBackend::handleMessageEvent(const std::shared_ptr<MessageViewEvent>& event)
{
    FeiqMessage message;
    message.fellow = toFellowInfo(event->fellow);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(event->when.time_since_epoch()).count();
    message.timestamp = QDateTime::fromMSecsSinceEpoch(ms);

    QVector<FeiqMessageContent> contents;
    contents.reserve(static_cast<int>(event->contents.size()));
    for (const auto& content : event->contents) {
        contents.append(toMessageContent(content));
    }
    message.contents = contents;

    QMetaObject::invokeMethod(this, [this, message]() {
        emit messageReceived(message);
    }, Qt::QueuedConnection);
}

void FeiqBackend::handleSendTimeoutEvent(const std::shared_ptr<SendTimeoEvent>& event)
{
    auto info = toFellowInfo(event->fellow);
    QString description;

    if (event->content && event->content->type() == ContentType::Text) {
        auto text = std::static_pointer_cast<TextContent>(event->content);
        description = tr("消息发送超时: %1").arg(QString::fromStdString(text->text));
    } else {
        description = tr("消息发送超时");
    }

    QMetaObject::invokeMethod(this, [this, info, description]() {
        emit sendTimeout(info, description);
    }, Qt::QueuedConnection);
}

std::shared_ptr<FileTask> FeiqBackend::findFileTask(quint32 packetNo, quint32 fileId, FileTaskType type) const
{
    return m_engine.getModel().findTask(packetNo, fileId, type);
}

bool FeiqBackend::isTestUser(const QString& ip) const
{
    return m_testUserEnabled && ip == m_testUser.ip;
}

void FeiqBackend::enableLoopbackTestUser(const QString& displayName)
{
    if (m_testUserEnabled) {
        return;
    }

    m_testUserEnabled = true;
    m_testUser.ip = QStringLiteral("127.0.0.2");
    m_testUser.host = QStringLiteral("loopback");
    m_testUser.name = displayName.isEmpty() ? QStringLiteral("测试用户") : displayName;
    m_testUser.mac = QStringLiteral("00:00:00:00:00:01");
    m_testUser.online = true;

    emit fellowUpdated(m_testUser);

    QTimer::singleShot(300, this, [this]() {
        if (m_testUserEnabled) {
            simulateTestUserIncomingText(tr("您好，我是测试用户，欢迎体验飞秋功能。"));
        }
    });
}

void FeiqBackend::simulateTestUserIncomingText(const QString& text)
{
    if (!m_testUserEnabled) {
        return;
    }

    FeiqMessage message;
    message.fellow = m_testUser;
    message.timestamp = QDateTime::currentDateTime();

    FeiqMessageContent content;
    content.type = FeiqContentType::Text;
    content.text = text;
    message.contents.append(content);

    QMetaObject::invokeMethod(this, [this, message]() {
        emit messageReceived(message);
    }, Qt::QueuedConnection);
}

void FeiqBackend::simulateTestUserIncomingFile(const QString& fileName, qint64 fileSize, const QByteArray& data)
{
    if (!m_testUserEnabled) {
        return;
    }

    QByteArray payload = data;
    if (payload.isEmpty()) {
        QByteArray templateData = QByteArrayLiteral("Test file content generated at ");
        templateData.append(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
        templateData.append('\n');
        payload = templateData;
    }

    FeiqFileOffer offer;
    offer.packetNo = m_testPacketCounter++;
    offer.fileId = m_testFileIdCounter++;
    offer.fileName = fileName.isEmpty() ? QStringLiteral("test.txt") : fileName;
    offer.fileSize = (fileSize >= 0) ? fileSize : payload.size();
    offer.fileType = 0;

    quint64 key = (static_cast<quint64>(offer.packetNo) << 32) | offer.fileId;
    m_testIncomingFiles.insert(key, payload);

    FeiqMessage message;
    message.fellow = m_testUser;
    message.timestamp = QDateTime::currentDateTime();

    FeiqMessageContent content;
    content.type = FeiqContentType::File;
    content.file = offer;
    message.contents.append(content);

    QMetaObject::invokeMethod(this, [this, message]() {
        emit messageReceived(message);
    }, Qt::QueuedConnection);
}

void FeiqBackend::emitTestUserMessage(const QString& text)
{
    simulateTestUserIncomingText(text);
}

void FeiqBackend::emitTestUserFileOffer(const FeiqFileOffer& offer)
{
    if (!m_testUserEnabled) {
        return;
    }

    FeiqMessage message;
    message.fellow = m_testUser;
    message.timestamp = QDateTime::currentDateTime();

    FeiqMessageContent content;
    content.type = FeiqContentType::File;
    content.file = offer;
    message.contents.append(content);

    QMetaObject::invokeMethod(this, [this, message]() {
        emit messageReceived(message);
    }, Qt::QueuedConnection);
}

bool FeiqBackend::handleTestUserSendText(const QString& text, QString* error)
{
    Q_UNUSED(error);

    QTimer::singleShot(250, this, [this, text]() {
        if (m_testUserEnabled) {
            QString reply = tr("[测试用户回声] %1").arg(text);
            simulateTestUserIncomingText(reply);
        }
    });

    return true;
}

bool FeiqBackend::handleTestUserSendFiles(const QStringList& filePaths, QString* error)
{
    if (filePaths.isEmpty()) {
        return true;
    }

    QVector<QPair<QString, QByteArray>> filesToEcho;
    filesToEcho.reserve(filePaths.size());

    for (const auto& path : filePaths) {
        QFile file(path);
        if (!file.exists()) {
            if (error) {
                *error = tr("文件不存在: %1").arg(path);
            }
            return false;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            if (error) {
                *error = tr("无法读取文件: %1").arg(path);
            }
            return false;
        }

        QByteArray fileData = file.readAll();
        file.close();

        QFileInfo info(path);

        FeiqFileTaskInfo taskInfo;
        taskInfo.upload = true;
        taskInfo.state = FeiqFileTaskState::Finished;
        taskInfo.fellow = m_testUser;
        taskInfo.file.fileName = info.fileName();
        taskInfo.file.localPath = info.absoluteFilePath();
        taskInfo.file.fileSize = info.size();
        taskInfo.detail = tr("测试用户已接收文件");

        QMetaObject::invokeMethod(this, [this, taskInfo]() {
            emit fileTaskUpdated(taskInfo);
        }, Qt::QueuedConnection);

        filesToEcho.append(qMakePair(info.fileName(), fileData));
    }

    QTimer::singleShot(300, this, [this]() {
        if (m_testUserEnabled) {
            simulateTestUserIncomingText(tr("测试用户已收到您的文件，谢谢！"));
        }
    });

    if (!filesToEcho.isEmpty()) {
        QTimer::singleShot(600, this, [this, filesToEcho]() {
            if (!m_testUserEnabled) {
                return;
            }

            for (const auto& item : filesToEcho) {
                simulateTestUserIncomingFile(item.first, static_cast<qint64>(item.second.size()), item.second);
            }
        });
    }

    return true;
}

bool FeiqBackend::handleTestUserAcceptFile(quint32 packetNo, quint32 fileId, const QString& savePath, QString* error)
{
    quint64 key = (static_cast<quint64>(packetNo) << 32) | fileId;
    if (!m_testIncomingFiles.contains(key)) {
        if (error) {
            *error = tr("未找到测试文件任务");
        }
        return false;
    }

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = tr("无法写入测试文件: %1").arg(savePath);
        }
        return false;
    }

    QByteArray data = m_testIncomingFiles.take(key);
    file.write(data);
    file.close();

    FeiqFileTaskInfo taskInfo;
    taskInfo.upload = false;
    taskInfo.state = FeiqFileTaskState::Finished;
    taskInfo.fellow = m_testUser;
    taskInfo.file.packetNo = packetNo;
    taskInfo.file.fileId = fileId;
    taskInfo.file.fileName = QFileInfo(savePath).fileName();
    taskInfo.file.localPath = savePath;
    taskInfo.file.fileSize = data.size();
    taskInfo.detail = tr("测试文件已保存");

    QMetaObject::invokeMethod(this, [this, taskInfo]() {
        emit fileTaskUpdated(taskInfo);
    }, Qt::QueuedConnection);

    simulateTestUserIncomingText(tr("测试用户提示: 文件已传送到 %1").arg(savePath));

    return true;
}

