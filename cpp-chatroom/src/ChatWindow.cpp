#include "ChatWindow.h"
#include "MainWindow.h"
#include "FileTransferWorker.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QThread>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

ChatWindow::ChatWindow(const QString& ownUsername, 
                       const IPMsg::MessagePacket& targetUserInfo,
                       const QString& targetIp,
                       NetworkManager* networkManager,
                       MainWindow* mainWindow,
                       QWidget* parent)
    : QWidget(parent)
    , m_ownUsername(ownUsername)
    , m_targetUserInfo(targetUserInfo)
    , m_targetIp(targetIp)
    , m_networkManager(networkManager)
    , m_mainWindow(mainWindow)
    , m_messageDisplay(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_fileButton(nullptr)
    , m_receiverThread(nullptr)
    , m_senderThread(nullptr)
    , m_receiverWorker(nullptr)
    , m_senderWorker(nullptr)
    , m_currentRequestPacketNo(0)
{
    QString senderName = targetUserInfo.displayName.isEmpty() ? 
        targetUserInfo.sender : targetUserInfo.displayName;
    setWindowTitle(QString("与 %1 聊天中").arg(senderName));
    setGeometry(300, 300, 500, 400);
    
    setupUI();
}

ChatWindow::~ChatWindow()
{
    if (m_receiverThread) {
        m_receiverThread->quit();
        m_receiverThread->wait();
    }
    if (m_senderThread) {
        m_senderThread->quit();
        m_senderThread->wait();
    }
}

void ChatWindow::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // 消息显示区域
    m_messageDisplay = new QTextEdit(this);
    m_messageDisplay->setReadOnly(true);
    layout->addWidget(m_messageDisplay);
    
    // 消息输入区域
    m_messageInput = new QTextEdit(this);
    m_messageInput->setFixedHeight(100);
    layout->addWidget(m_messageInput);
    
    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_fileButton = new QPushButton("发送文件", this);
    m_sendButton = new QPushButton("发送", this);
    QPushButton* closeButton = new QPushButton("关闭", this);
    
    buttonLayout->addWidget(m_fileButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    buttonLayout->addWidget(m_sendButton);
    
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(m_fileButton, &QPushButton::clicked, this, &ChatWindow::onFileClicked);
    connect(closeButton, &QPushButton::clicked, this, &ChatWindow::close);
}

void ChatWindow::appendMessage(const QString& text, const QString& senderName, bool isOwn)
{
    QString senderHtml;
    if (isOwn) {
        senderHtml = QString("<p style=\"color: green; margin-bottom: 0;\"><b>%1 (我):</b></p>")
            .arg(senderName);
    } else {
        senderHtml = QString("<p style=\"color: blue; margin-bottom: 0;\"><b>%1:</b></p>")
            .arg(senderName);
    }
    
    QString contentHtml = formatTextForDisplay(text);
    QString fullHtml = QString("%1<div style=\"margin-left: 10px;\">%2</div>")
        .arg(senderHtml).arg(contentHtml);
    
    m_messageDisplay->append(fullHtml);
}

QString ChatWindow::formatTextForDisplay(const QString& text)
{
    QString formatted = text;
    formatted.replace("&", "&amp;");
    formatted.replace("<", "&lt;");
    formatted.replace(">", "&gt;");
    formatted.replace("\n", "<br>");
    return formatted;
}

void ChatWindow::onSendClicked()
{
    QString messageText = m_messageInput->toPlainText();
    if (messageText.trimmed().isEmpty()) {
        return;
    }
    
    appendMessage(messageText, m_ownUsername, true);
    m_networkManager->sendMessage(messageText, m_targetIp);
    m_messageInput->clear();
}

void ChatWindow::onFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的文件");
    if (!filePath.isEmpty()) {
        emit sendFileRequest(m_targetIp, filePath);
    }
}

void ChatWindow::handleFileRequest(const IPMsg::MessagePacket& msg, const QString& senderIp)
{
    QStringList parts = msg.extraMsg.split(':');
    if (parts.size() < 2) {
        qWarning() << "文件请求消息格式错误";
        return;
    }
    
    QString filename = parts[0];
    bool ok;
    qint64 filesize = parts[1].toLongLong(&ok);
    if (!ok) {
        qWarning() << "文件大小解析失败";
        return;
    }
    
    QString senderName = m_targetUserInfo.displayName.isEmpty() ? 
        m_targetUserInfo.sender : m_targetUserInfo.displayName;
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, 
        "文件传输请求",
        QString("用户 %1 (%2) 想传送文件:\n名称: %3\n大小: %4 bytes\n\n您是否同意接收？")
            .arg(senderName).arg(senderIp).arg(filename).arg(filesize),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // 保存当前请求的包ID
        m_currentRequestPacketNo = msg.packetNo;
        
        // 创建缓存目录
        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir dir(cacheDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        
        QString savePath = QDir(cacheDir).filePath(filename);
        
        // 创建文件接收工作线程
        m_receiverThread = new QThread(this);
        m_receiverWorker = new FileTransferWorker();
        m_receiverWorker->moveToThread(m_receiverThread);
        
        connect(m_receiverThread, &QThread::started, [this, savePath, filesize]() {
            m_receiverWorker->startReceiving(savePath, filesize);
        });
        
        connect(m_receiverWorker, &FileTransferWorker::readyToReceive, 
                this, &ChatWindow::onReceiverReady);
        connect(m_receiverWorker, &FileTransferWorker::receiveFinished, 
                this, &ChatWindow::onReceiveFinished);
        connect(m_receiverWorker, &FileTransferWorker::receiveError, 
                this, &ChatWindow::onReceiveError);
        
        connect(m_receiverThread, &QThread::finished, m_receiverWorker, &QObject::deleteLater);
        
        m_receiverThread->start();
    }
}

void ChatWindow::handleFileReady(const IPMsg::MessagePacket& msg, const QString& senderIp)
{
    Q_UNUSED(senderIp);
    
    QStringList parts = msg.extraMsg.split(':');
    if (parts.size() < 2) {
        qWarning() << "文件就绪消息格式错误";
        return;
    }
    
    bool ok;
    quint16 tcpPort = parts[0].toUShort(&ok);
    if (!ok) {
        qWarning() << "TCP端口解析失败";
        return;
    }
    
    quint32 originalPacketNo = parts[1].toUInt(&ok);
    if (!ok) {
        qWarning() << "包ID解析失败";
        return;
    }
    
    // 从待发送文件列表中找到对应的文件路径
    QString filepath = m_pendingFiles.value(originalPacketNo);
    if (filepath.isEmpty()) {
        qWarning() << QString("找不到包ID %1 对应的文件").arg(originalPacketNo);
        return;
    }
    
    qDebug() << QString("对方已准备就绪，开始通过TCP传送文件 %1 到 %2:%3")
        .arg(filepath).arg(senderIp).arg(tcpPort);
    
    // 创建文件发送工作线程
    m_senderThread = new QThread(this);
    m_senderWorker = new FileTransferWorker();
    m_senderWorker->moveToThread(m_senderThread);
    
    connect(m_senderThread, &QThread::started, [this, filepath, senderIp, tcpPort]() {
        m_senderWorker->startSending(filepath, senderIp, tcpPort);
    });
    
    connect(m_senderWorker, &FileTransferWorker::sendFinished, 
            this, &ChatWindow::onSendFinished);
    connect(m_senderWorker, &FileTransferWorker::sendError, 
            this, &ChatWindow::onSendError);
    
    connect(m_senderThread, &QThread::finished, m_senderWorker, &QObject::deleteLater);
    
    m_senderThread->start();
    
    // 发送后从列表中移除
    m_pendingFiles.remove(originalPacketNo);
}

void ChatWindow::setPendingFile(quint32 packetNo, const QString& filePath)
{
    m_pendingFiles[packetNo] = filePath;
}

void ChatWindow::setCurrentRequestPacketNo(quint32 packetNo)
{
    m_currentRequestPacketNo = packetNo;
}

void ChatWindow::onReceiverReady(quint16 tcpPort, const QString& savePath)
{
    Q_UNUSED(savePath);
    // 发送UDP就绪信号，使用当前请求的包ID
    emit sendFileReady(tcpPort, m_currentRequestPacketNo, m_targetIp);
}

void ChatWindow::onReceiveFinished(const QString& filePath)
{
    appendMessage(QString("文件已接收: %1").arg(filePath), m_targetUserInfo.displayName);
    QMessageBox::information(this, "完成", QString("文件已成功接收: %1").arg(filePath));
}

void ChatWindow::onReceiveError(const QString& error)
{
    QMessageBox::warning(this, "错误", QString("文件接收失败: %1").arg(error));
}

void ChatWindow::onSendFinished()
{
    QMessageBox::information(this, "完成", "文件已成功发送");
}

void ChatWindow::onSendError(const QString& error)
{
    QMessageBox::warning(this, "错误", QString("文件发送失败: %1").arg(error));
}

void ChatWindow::onReceiveProgress(qint64 bytesReceived, qint64 totalBytes)
{
    Q_UNUSED(bytesReceived);
    Q_UNUSED(totalBytes);
    // 可以在这里更新进度条
}

void ChatWindow::onSendProgress(qint64 bytesSent, qint64 totalBytes)
{
    Q_UNUSED(bytesSent);
    Q_UNUSED(totalBytes);
    // 可以在这里更新进度条
}

void ChatWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << QString("关闭与 %1 的聊天窗口").arg(m_targetIp);
    event->accept();
}

