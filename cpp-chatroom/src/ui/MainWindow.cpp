#include "ui/MainWindow.h"
#include "ui/ChatWindow.h"
#include "ui/SettingsDialog.h"
#include "ui/AboutDialog.h"
#include "ui/GroupChatDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHostInfo>
#include <QFileDialog>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QDateTime>
#include <QInputDialog>
#include <algorithm>

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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_userTreeWidget(nullptr)
    , m_searchInput(nullptr)
    , m_statusLabel(nullptr)
    , m_groupMessageButton(nullptr)
    , m_fileMenu(nullptr)
    , m_helpMenu(nullptr)
    , m_testMenu(nullptr)
    , m_settingsAction(nullptr)
    , m_exitAction(nullptr)
    , m_aboutAction(nullptr)
    , m_testSendMessageAction(nullptr)
    , m_testSendFileAction(nullptr)
    , m_backend(new FeiqBackend(this))
{
    loadSettings();
    
    setupUI();
    setupMenuBar();

    m_backend->setIdentity(m_username, m_hostname);
    setupConnections();

    if (!m_backend->start()) {
        QMessageBox::critical(this, tr("错误"), tr("无法启动飞秋协议服务，请检查网络设置。"));
    } else {
        m_statusLabel->setText(tr("在线用户: %1").arg(m_users.size()));
        m_backend->enableLoopbackTestUser();
    }
}

MainWindow::~MainWindow()
{
    // 关闭所有聊天窗口
    for (ChatWindow* chatWin : m_chatWindows.values()) {
        if (chatWin) {
            chatWin->close();
        }
    }
    m_chatWindows.clear();
    
    if (m_backend) {
        m_backend->stop();
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("FeiQ Chatroom");
    setGeometry(200, 200, 300, 500);
    
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    
    // 搜索框
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("搜索用户、IP...");
    layout->addWidget(m_searchInput);
    
    // 用户列表
    m_userTreeWidget = new QTreeWidget(this);
    m_userTreeWidget->setHeaderLabels(QStringList() << "在线好友");
    m_userTreeWidget->setIndentation(15);
    m_userTreeWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_userTreeWidget);
    
    // 底部栏
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    m_groupMessageButton = new QPushButton("群发消息", this);
    bottomLayout->addWidget(m_groupMessageButton);
    bottomLayout->addStretch();
    m_statusLabel = new QLabel("正在初始化...", this);
    bottomLayout->addWidget(m_statusLabel);
    layout->addLayout(bottomLayout);
}

void MainWindow::setupMenuBar()
{
    m_fileMenu = menuBar()->addMenu("文件(&F)");
    
    m_settingsAction = new QAction("设置(&S)...", this);
    m_settingsAction->setShortcut(QKeySequence::Preferences);
    m_fileMenu->addAction(m_settingsAction);
    
    m_fileMenu->addSeparator();
    
    m_exitAction = new QAction("退出(&X)", this);
    m_exitAction->setShortcut(QKeySequence::Quit);
    m_fileMenu->addAction(m_exitAction);
    
    m_helpMenu = menuBar()->addMenu("帮助(&H)");
    m_aboutAction = new QAction("关于(&A)...", this);
    m_helpMenu->addAction(m_aboutAction);

    m_testMenu = menuBar()->addMenu("测试(&T)");
    m_testSendMessageAction = new QAction("测试用户发送消息", this);
    m_testSendFileAction = new QAction("测试用户发送文件", this);
    m_testMenu->addAction(m_testSendMessageAction);
    m_testMenu->addAction(m_testSendFileAction);
    
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);
    connect(m_exitAction, &QAction::triggered, this, &QWidget::close);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAboutClicked);
    connect(m_testSendMessageAction, &QAction::triggered, this, &MainWindow::onTestUserSendMessage);
    connect(m_testSendFileAction, &QAction::triggered, this, &MainWindow::onTestUserSendFile);
}

void MainWindow::loadSettings()
{
    QSettings settings("FeiQChatroom", "Settings");
    
    m_username = settings.value("username", "CppUser").toString();
    m_hostname = settings.value("hostname", "").toString();
    m_groupname = settings.value("groupname", "我的好友").toString();
    
    if (m_hostname.isEmpty()) {
        m_hostname = QHostInfo::localHostName();
    }
}

void MainWindow::setupConnections()
{
    // 网络信号
    connect(m_backend, &FeiqBackend::fellowUpdated, this, &MainWindow::handleFellowUpdated);
    connect(m_backend, &FeiqBackend::messageReceived, this, &MainWindow::handleMessageReceived);
    connect(m_backend, &FeiqBackend::sendTimeout, this, &MainWindow::handleSendTimeout);
    connect(m_backend, &FeiqBackend::fileTaskUpdated, this, &MainWindow::handleFileTaskUpdated);
    connect(m_backend, &FeiqBackend::engineError, this, [this](const QString& message) {
        QMessageBox::critical(this, tr("错误"), message);
    });

    // UI信号
    connect(m_userTreeWidget, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onUserItemDoubleClicked);
    connect(m_userTreeWidget, &QTreeWidget::itemActivated,
            this, &MainWindow::onUserItemActivated);
    connect(m_searchInput, &QLineEdit::textChanged, 
            this, &MainWindow::onSearchTextChanged);
    connect(m_groupMessageButton, &QPushButton::clicked, 
            this, &MainWindow::onGroupMessageClicked);
}

void MainWindow::handleFellowUpdated(const FeiqFellowInfo& fellow)
{
    if (fellow.ip.isEmpty()) {
        return;
    }

    if (fellow.online) {
        m_users[fellow.ip] = fellow;
    } else {
        if (m_chatWindows.contains(fellow.ip)) {
            m_chatWindows[fellow.ip]->appendText(tr("对方已离线"), displayNameOf(fellow));
            m_chatWindows[fellow.ip]->close();
        }
        m_users.remove(fellow.ip);
    }

    refreshUserNode(fellow);
}

void MainWindow::handleMessageReceived(const FeiqMessage& message)
{
    const QString ip = message.fellow.ip;
    const QString senderName = displayNameOf(message.fellow);

    if (!m_chatWindows.contains(ip) || !m_chatWindows[ip]->isVisible()) {
        openChatWindow(ip);
    }

    for (const auto& content : message.contents) {
        switch (content.type) {
        case FeiqContentType::Text:
            appendTextToChat(ip, content.text, senderName, false);
            break;
        case FeiqContentType::File:
            appendFileOfferToChat(ip, content.file, senderName);
            promptFileDownload(ip, content.file, senderName);
            break;
        case FeiqContentType::Knock:
            appendTextToChat(ip, tr("对方向你发起了窗口抖动"), senderName, false);
            break;
        case FeiqContentType::Image:
            appendTextToChat(ip, tr("收到图片，请对方以文件形式发送"), senderName, false);
            break;
        case FeiqContentType::Id:
            break;
        }
    }
}

void MainWindow::handleSendTimeout(const FeiqFellowInfo& fellow, const QString& description)
{
    appendTextToChat(fellow.ip, description, m_username, true);
}

void MainWindow::handleFileTaskUpdated(const FeiqFileTaskInfo& info)
{
    const QString ip = info.fellow.ip;
    if (ip.isEmpty()) {
        return;
    }

    QString actor = info.upload ? m_username : displayNameOf(info.fellow);
    bool isOwn = info.upload;

    switch (info.state) {
    case FeiqFileTaskState::Running:
        // We could update progress in future.
        break;
    case FeiqFileTaskState::Finished: {
        QString message;
        if (info.upload) {
            message = tr("文件已发送: %1").arg(info.file.fileName);
        } else {
            const QString pathHint = info.file.localPath.isEmpty()
                ? info.file.fileName
                : info.file.localPath;
            message = tr("文件已接收: %1").arg(pathHint);
        }
        appendTextToChat(ip, message, actor, isOwn);
        break;
    }
    case FeiqFileTaskState::Error:
        appendTextToChat(ip,
                         tr("文件传输失败: %1").arg(info.detail.isEmpty() ? info.file.fileName : info.detail),
                         actor,
                         isOwn);
        break;
    case FeiqFileTaskState::Canceled:
        appendTextToChat(ip, tr("文件传输已取消: %1").arg(info.file.fileName), actor, isOwn);
        break;
    case FeiqFileTaskState::NotStart:
        break;
    }
}

void MainWindow::onUserItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) {
        return;
    }

    QMap<QString, QVariant> itemData = data.toMap();
    if (itemData["type"].toString() != "user") {
        return;
    }

    QString ip = itemData.value("ip").toString();
    openChatWindow(ip);
}

void MainWindow::onUserItemActivated(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.isValid()) {
        return;
    }

    QMap<QString, QVariant> itemData = data.toMap();
    if (itemData["type"].toString() != "user") {
        return;
    }

    QString ip = itemData.value("ip").toString();
    openChatWindow(ip);
}

void MainWindow::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
    filterUserList();
}

void MainWindow::onGroupMessageClicked()
{
    QTreeWidgetItem* selectedItem = m_userTreeWidget->currentItem();
    if (!selectedItem) {
        QMessageBox::warning(this, "操作提示", "请先在列表中选择一个分组或一位用户。");
        return;
    }
    
    QString groupName = tr("在线好友");

    QList<QPair<QString, QString>> recipients;
    for (const auto& key : m_users.keys()) {
        recipients.append(qMakePair(key, displayNameOf(m_users[key])));
    }

    if (recipients.isEmpty()) {
        QMessageBox::information(this, "提示", "该分组中没有可发送消息的用户。");
        return;
    }
    
    GroupChatDialog* dialog = new GroupChatDialog(m_username, groupName, recipients,
                                                  m_backend, this, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

void MainWindow::onSendFileRequest(const QString& targetIp, const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::critical(this, "错误", QString("文件不存在: %1").arg(filePath));
        return;
    }
    
    QString errorMessage;
    if (!m_backend->sendFiles(targetIp, QStringList{filePath}, &errorMessage)) {
        QMessageBox::critical(this, tr("错误"), errorMessage);
    } else if (m_chatWindows.contains(targetIp)) {
        m_chatWindows[targetIp]->appendText(QString("已请求发送文件: %1").arg(fileInfo.fileName()),
                                            m_username,
                                            true);
    }
}

void MainWindow::updateUserList()
{
    m_userTreeWidget->clear();

    QTreeWidgetItem* groupItem = new QTreeWidgetItem(m_userTreeWidget);
    groupItem->setText(0, tr("在线好友 [%1]").arg(m_users.size()));
    groupItem->setData(0, Qt::UserRole, QVariant::fromValue(QString("group")));

    QList<QString> sortedIps = m_users.keys();
    std::sort(sortedIps.begin(), sortedIps.end(), [this](const QString& a, const QString& b) {
        return displayNameOf(m_users[a]) < displayNameOf(m_users[b]);
    });

    for (const QString& ip : sortedIps) {
        const auto& fellow = m_users[ip];
        QTreeWidgetItem* userItem = new QTreeWidgetItem(groupItem);
        userItem->setText(0, QString("%1 (%2)").arg(displayNameOf(fellow), ip));
        QMap<QString, QVariant> userData;
        userData["type"] = "user";
        userData["ip"] = ip;
        userItem->setData(0, Qt::UserRole, userData);
    }

    m_userTreeWidget->expandItem(groupItem);
    m_statusLabel->setText(QString("在线用户: %1").arg(m_users.size()));
    filterUserList();
}

void MainWindow::filterUserList()
{
    QString searchText = m_searchInput->text().toLower();
    QTreeWidgetItem* root = m_userTreeWidget->invisibleRootItem();
    
    for (int i = 0; i < root->childCount(); ++i) {
        QTreeWidgetItem* groupItem = root->child(i);
        bool hasVisibleChild = false;
        
        for (int j = 0; j < groupItem->childCount(); ++j) {
            QTreeWidgetItem* userItem = groupItem->child(j);
            QString itemText = userItem->text(0).toLower();
            if (searchText.isEmpty() || itemText.contains(searchText)) {
                userItem->setHidden(false);
                hasVisibleChild = true;
            } else {
                userItem->setHidden(true);
            }
        }
        
        groupItem->setHidden(!hasVisibleChild);
    }
    
}

void MainWindow::openChatWindow(const QString& targetIp)
{
    if (m_chatWindows.contains(targetIp) && m_chatWindows[targetIp]->isVisible()) {
        m_chatWindows[targetIp]->activateWindow();
        return;
    }
    
    FeiqFellowInfo fellow = m_users.value(targetIp, FeiqFellowInfo{targetIp, targetIp, QString(), QString(), true});
    ChatWindow* chatWin = new ChatWindow(m_username, fellow, m_backend, this, nullptr);

    chatWin->setAttribute(Qt::WA_DeleteOnClose);

    ensureChatWindowSignals(chatWin);

    m_chatWindows[targetIp] = chatWin;
    chatWin->show();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (ChatWindow* chatWin : m_chatWindows.values()) {
        if (chatWin) {
            chatWin->close();
        }
    }
    
    if (m_backend) {
        m_backend->stop();
    }
    
    event->accept();
}

void MainWindow::onSettingsClicked()
{
    SettingsDialog* dialog = new SettingsDialog(this);
    
    // 设置当前值
    dialog->setUsername(m_username);
    dialog->setHostname(m_hostname);
    dialog->setGroupname(m_groupname);
    
    if (dialog->exec() == QDialog::Accepted) {
        // 保存设置
        m_username = dialog->username();
        m_hostname = dialog->hostname();
        m_groupname = dialog->groupname();
        
        if (m_backend) {
            m_backend->stop();
            m_backend->setIdentity(m_username, m_hostname);
            if (!m_backend->start()) {
                QMessageBox::critical(this, tr("错误"), tr("无法重新启动飞秋协议服务。"));
            }
        }

        m_users.clear();
        updateUserList();
    }
    
    dialog->deleteLater();
}

void MainWindow::onAboutClicked()
{
    AboutDialog* dialog = new AboutDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}

void MainWindow::onTestUserSendMessage()
{
    if (!m_backend) {
        return;
    }

    bool ok = false;
    QString text = QInputDialog::getText(this,
                                         tr("测试用户消息"),
                                         tr("输入测试用户要发送的消息:"),
                                         QLineEdit::Normal,
                                         tr("你好！这是测试用户。"),
                                         &ok);
    if (!ok) {
        return;
    }

    text = text.trimmed();
    if (text.isEmpty()) {
        return;
    }

    m_backend->simulateTestUserIncomingText(text);
}

void MainWindow::onTestUserSendFile()
{
    if (!m_backend) {
        return;
    }

    bool ok = false;
    QString fileName = QInputDialog::getText(this,
                                             tr("测试用户文件"),
                                             tr("输入测试文件名:"),
                                             QLineEdit::Normal,
                                             tr("loopback.txt"),
                                             &ok);
    if (!ok) {
        return;
    }

    fileName = fileName.trimmed();
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("loopback.txt");
    }

    m_backend->simulateTestUserIncomingFile(fileName);
}

void MainWindow::appendTextToChat(const QString& targetIp, const QString& message,
                                  const QString& senderName, bool isOwn)
{
    if (!m_chatWindows.contains(targetIp)) {
        openChatWindow(targetIp);
    }

    if (m_chatWindows.contains(targetIp)) {
        QString emoji;
        if (ChatWindow::isEmojiMessage(message, &emoji)) {
            m_chatWindows[targetIp]->appendEmoji(emoji, senderName, isOwn);
        } else {
            m_chatWindows[targetIp]->appendText(message, senderName, isOwn);
        }
    }
}

void MainWindow::appendFileOfferToChat(const QString& targetIp, const FeiqFileOffer& offer,
                                       const QString& senderName)
{
    if (!m_chatWindows.contains(targetIp)) {
        openChatWindow(targetIp);
    }

    if (m_chatWindows.contains(targetIp)) {
        m_chatWindows[targetIp]->appendFileOffer(offer, senderName);
    }
}

void MainWindow::promptFileDownload(const QString& targetIp, const FeiqFileOffer& offer,
                                    const QString& senderName)
{
    auto response = QMessageBox::question(
        this,
        tr("接收文件"),
        tr("%1 想要发送文件 %2 (%3 字节)。是否接收？")
            .arg(senderName)
            .arg(offer.fileName)
            .arg(offer.fileSize));

    if (response != QMessageBox::Yes) {
        return;
    }

    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (defaultDir.isEmpty()) {
        defaultDir = QDir::homePath();
    }

    QString savePath = QFileDialog::getSaveFileName(
        this,
        tr("保存文件"),
        QDir(defaultDir).filePath(offer.fileName));

    if (savePath.isEmpty()) {
        return;
    }

    QString errorMessage;
    if (!m_backend->acceptFile(targetIp, offer.packetNo, offer.fileId, savePath, &errorMessage)) {
        QMessageBox::critical(this, tr("错误"), errorMessage);
    }
}

void MainWindow::refreshUserNode(const FeiqFellowInfo& fellow)
{
    Q_UNUSED(fellow);
    updateUserList();
}

void MainWindow::ensureChatWindowSignals(ChatWindow* chatWindow)
{
    connect(chatWindow, &ChatWindow::sendFileRequest,
            this, &MainWindow::onSendFileRequest);

    connect(chatWindow, &ChatWindow::destroyed, this, [this, chatWindow]() {
        for (auto it = m_chatWindows.begin(); it != m_chatWindows.end(); ++it) {
            if (it.value() == chatWindow) {
                m_chatWindows.erase(it);
                break;
            }
        }
    });
}

