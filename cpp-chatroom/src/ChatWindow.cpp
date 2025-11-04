#include "ChatWindow.h"
#include "MainWindow.h"
#include "ScreenshotTool.h"
#include "FeiqBackend.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QCloseEvent>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QImageReader>
#include <QTextImageFormat>
#include <QTextCursor>
#include <QFileInfo>
#include <QApplication>
#include <QTimer>
#include <QDateTime>

namespace
{

QString displayNameOf(const FeiqFellowInfo& fellow)
{
    if (!fellow.name.trimmed().isEmpty()) {
        return fellow.name;
    }
    if (!fellow.host.trimmed().isEmpty()) {
        return fellow.host;
    }
    return fellow.ip;
}

}

ChatWindow::ChatWindow(const QString& ownUsername,
                       const FeiqFellowInfo& targetFellow,
                       FeiqBackend* backend,
                       MainWindow* mainWindow,
                       QWidget* parent)
    : QWidget(parent)
    , m_ownUsername(ownUsername)
    , m_targetFellow(targetFellow)
    , m_targetIp(targetFellow.ip)
    , m_backend(backend)
    , m_mainWindow(mainWindow)
    , m_messageDisplay(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_fileButton(nullptr)
    , m_imageButton(nullptr)
    , m_screenshotButton(nullptr)
    , m_screenshotTool(nullptr)
{
    setWindowTitle(QStringLiteral("与 %1 聊天中").arg(displayNameOf(targetFellow)));
    setGeometry(300, 300, 500, 400);
    
    setupUI();
}

ChatWindow::~ChatWindow() = default;

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
    
    // 工具栏按钮
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    m_fileButton = new QPushButton("发送文件", this);
    m_imageButton = new QPushButton("发送图片", this);
    m_screenshotButton = new QPushButton("截图", this);
    toolbarLayout->addWidget(m_fileButton);
    toolbarLayout->addWidget(m_imageButton);
    toolbarLayout->addWidget(m_screenshotButton);
    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);
    
    // 按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_sendButton = new QPushButton("发送", this);
    QPushButton* closeButton = new QPushButton("关闭", this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    buttonLayout->addWidget(m_sendButton);
    
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_sendButton, &QPushButton::clicked, this, &ChatWindow::onSendClicked);
    connect(m_fileButton, &QPushButton::clicked, this, &ChatWindow::onFileClicked);
    connect(m_imageButton, &QPushButton::clicked, this, &ChatWindow::onImageClicked);
    connect(m_screenshotButton, &QPushButton::clicked, this, &ChatWindow::onScreenshotClicked);
    connect(closeButton, &QPushButton::clicked, this, &ChatWindow::close);
}

void ChatWindow::appendText(const QString& text, const QString& senderName, bool isOwn)
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
    
    QString errorMessage;
    if (m_backend && m_backend->sendText(m_targetIp, messageText, QString(), &errorMessage)) {
        appendText(messageText, m_ownUsername, true);
        m_messageInput->clear();
    } else {
        if (errorMessage.isEmpty()) {
            errorMessage = tr("无法发送消息");
        }
        QMessageBox::warning(this, tr("发送失败"), errorMessage);
    }
}

void ChatWindow::onFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的文件");
    if (!filePath.isEmpty()) {
        emit sendFileRequest(m_targetIp, filePath);
    }
}

void ChatWindow::onImageClicked()
{
    QStringList filters;
    filters << "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.ico)"
            << "PNG文件 (*.png)"
            << "JPEG文件 (*.jpg *.jpeg)"
            << "所有文件 (*.*)";
    
    QString filePath = QFileDialog::getOpenFileName(this, "选择要发送的图片", 
                                                     "", filters.join(";;"));
    if (!filePath.isEmpty()) {
        QFileInfo fileInfo(filePath);
        // 检查是否为有效的图片文件
        QImageReader reader(filePath);
        if (!reader.canRead()) {
            QMessageBox::warning(this, "错误", "无法读取该图片文件！");
            return;
        }
        
        // 显示图片并发送
        appendImage(filePath, m_ownUsername, true);
        emit sendFileRequest(m_targetIp, filePath);
    }
}

void ChatWindow::onScreenshotClicked()
{
    // 在创建截图工具之前，先隐藏当前窗口
    if (m_mainWindow) {
        m_mainWindow->hide();
    }
    hide();
    
    // 处理事件，确保窗口完全隐藏
    QApplication::processEvents();
    
    // 使用QTimer延迟创建截图工具，确保窗口完全隐藏后再截图
    QTimer::singleShot(200, [this]() {
        // 创建截图工具（此时会自动截图）
        m_screenshotTool = new ScreenshotTool();
        connect(m_screenshotTool, &ScreenshotTool::screenshotTaken, 
                this, &ChatWindow::onScreenshotTaken);
        connect(m_screenshotTool, &ScreenshotTool::destroyed, [this]() {
            if (m_mainWindow) {
                m_mainWindow->show();
            }
            show();
            activateWindow();
        });
        
        // 延迟显示截图工具，确保截图完成
        QTimer::singleShot(100, [this]() {
            if (m_screenshotTool) {
                m_screenshotTool->show();
                m_screenshotTool->raise();
                m_screenshotTool->activateWindow();
            }
        });
    });
}

void ChatWindow::onScreenshotTaken(const QString& filePath)
{
    if (m_screenshotTool) {
        m_screenshotTool->deleteLater();
        m_screenshotTool = nullptr;
    }
    
    if (m_mainWindow) {
        m_mainWindow->show();
    }
    show();
    activateWindow();
    
    // 显示截图并发送
    appendImage(filePath, m_ownUsername, true);
    emit sendFileRequest(m_targetIp, filePath);
}

void ChatWindow::appendImage(const QString& imagePath, const QString& senderName, bool isOwn)
{
    QString header;
    if (isOwn) {
        header = QString("<p style=\"color: green;\"><b>%1 (我) 发送了图片:</b></p>")
            .arg(senderName);
    } else {
        header = QString("<p style=\"color: blue;\"><b>%1 发送了图片:</b></p>")
            .arg(senderName);
    }
    
    QUrl imageUrl = QUrl::fromLocalFile(QFileInfo(imagePath).absoluteFilePath());
    QString imageHtml = QString("<div style=\"margin-left: 10px;\"><img src=\"%1\"></div>")
                            .arg(imageUrl.toString());

    m_messageDisplay->append(header + imageHtml);
    m_messageDisplay->append("");
    m_messageDisplay->ensureCursorVisible();
}

void ChatWindow::closeEvent(QCloseEvent* event)
{
    qDebug() << QString("关闭与 %1 的聊天窗口").arg(m_targetIp);
    event->accept();
}

void ChatWindow::appendFileOffer(const FeiqFileOffer& offer, const QString& senderName)
{
    const QString text = tr("%1 想发送文件: %2 (%3 字节)")
                            .arg(senderName)
                            .arg(offer.fileName)
                            .arg(offer.fileSize);
    appendText(text, senderName, false);
}

void ChatWindow::updateFellow(const FeiqFellowInfo& fellow)
{
    m_targetFellow = fellow;
    setWindowTitle(QStringLiteral("与 %1 聊天中").arg(displayNameOf(fellow)));
}

