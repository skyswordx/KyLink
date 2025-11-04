#include "MainWindow.h"
#include "ChatWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QHostInfo>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_userTreeWidget(nullptr)
    , m_searchInput(nullptr)
    , m_statusLabel(nullptr)
    , m_groupMessageButton(nullptr)
    , m_networkManager(nullptr)
{
    // 获取系统信息
    m_username = "CppUser";
    m_hostname = QHostInfo::localHostName();
    m_groupname = "开发组";
    
    setupUI();
    
    // 启动网络管理器（必须在setupConnections之前）
    m_networkManager = new NetworkManager(m_username, m_hostname, m_groupname, IPMsg::IPMSG_DEFAULT_PORT, this);
    setupConnections();
    
    if (!m_networkManager->start()) {
        QMessageBox::critical(this, "错误", "无法启动网络服务。端口可能已被占用。");
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
    
    if (m_networkManager) {
        m_networkManager->stop();
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

void MainWindow::setupConnections()
{
    // 网络信号
    connect(m_networkManager, &NetworkManager::userOnline, 
            this, &MainWindow::handleUserOnline);
    connect(m_networkManager, &NetworkManager::userOffline, 
            this, &MainWindow::handleUserOffline);
    connect(m_networkManager, &NetworkManager::messageReceived, 
            this, &MainWindow::handleMessageReceived);
    connect(m_networkManager, &NetworkManager::fileRequestReceived, 
            this, &MainWindow::handleFileRequestReceived);
    connect(m_networkManager, &NetworkManager::fileReceiverReady, 
            this, &MainWindow::handleFileReceiverReady);
    
    // UI信号
    connect(m_userTreeWidget, &QTreeWidget::itemDoubleClicked, 
            this, &MainWindow::onUserItemDoubleClicked);
    connect(m_searchInput, &QLineEdit::textChanged, 
            this, &MainWindow::onSearchTextChanged);
    connect(m_groupMessageButton, &QPushButton::clicked, 
            this, &MainWindow::onGroupMessageClicked);
}

void MainWindow::handleUserOnline(const IPMsg::MessagePacket& msg, const QString& ip)
{
    IPMsg::MessagePacket userInfo = msg;
    QString displayName = userInfo.displayName.isEmpty() ? userInfo.sender : userInfo.displayName;
    
    if (!m_users.contains(ip) || 
        m_users[ip].displayName != displayName || 
        m_users[ip].groupName != userInfo.groupName) {
        qDebug() << QString("用户上线或更新: %1 @ %2 in group '%3'")
            .arg(displayName).arg(ip).arg(userInfo.groupName);
        m_users[ip] = userInfo;
        updateUserList();
    }
}

void MainWindow::handleUserOffline(const IPMsg::MessagePacket& msg, const QString& ip)
{
    if (m_users.contains(ip)) {
        QString displayName = m_users[ip].displayName.isEmpty() ? 
            m_users[ip].sender : m_users[ip].displayName;
        qDebug() << QString("用户下线: %1 @ %2").arg(displayName).arg(ip);
        
        if (m_chatWindows.contains(ip)) {
            m_chatWindows[ip]->close();
        }
        
        m_users.remove(ip);
        updateUserList();
    }
}

void MainWindow::handleMessageReceived(const IPMsg::MessagePacket& msg, const QString& ip)
{
    QString senderName = msg.displayName.isEmpty() ? msg.sender : msg.displayName;
    qDebug() << QString("收到来自 %1 的消息: %2").arg(senderName).arg(msg.extraMsg);
    
    if (!m_chatWindows.contains(ip) || !m_chatWindows[ip]->isVisible()) {
        openChatWindow(ip);
    }
    
    if (m_chatWindows.contains(ip)) {
        m_chatWindows[ip]->appendMessage(msg.extraMsg, senderName);
        m_chatWindows[ip]->activateWindow();
    }
}

void MainWindow::handleFileRequestReceived(const IPMsg::MessagePacket& msg, const QString& ip)
{
    if (!m_chatWindows.contains(ip) || !m_chatWindows[ip]->isVisible()) {
        openChatWindow(ip);
    }
    
    if (m_chatWindows.contains(ip)) {
        m_chatWindows[ip]->activateWindow();
        m_chatWindows[ip]->handleFileRequest(msg, ip);
    }
}

void MainWindow::handleFileReceiverReady(const IPMsg::MessagePacket& msg, const QString& ip)
{
    if (m_chatWindows.contains(ip)) {
        m_chatWindows[ip]->handleFileReady(msg, ip);
    }
}

void MainWindow::onUserItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    QVariant data = item->data(0, Qt::UserRole);
    if (data.isValid()) {
        QMap<QString, QVariant> itemData = data.toMap();
        if (itemData["type"].toString() == "user") {
            QString ip = itemData["ip"].toString();
            openChatWindow(ip);
        }
    }
}

void MainWindow::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
    filterUserList();
}

void MainWindow::onGroupMessageClicked()
{
    QMessageBox::information(this, "提示", "群发消息功能暂未实现");
}

void MainWindow::onSendFileRequest(const QString& targetIp, const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        QMessageBox::critical(this, "错误", QString("文件不存在: %1").arg(filePath));
        return;
    }
    
    qint64 fileSize = fileInfo.size();
    QString filename = fileInfo.fileName();
    
    // 发送UDP文件请求
    m_networkManager->sendFileRequest(filename, fileSize, targetIp);
    
    // 将文件信息存入对应聊天窗口的待发送列表
    if (m_chatWindows.contains(targetIp)) {
        quint32 packetNo = m_networkManager->getPacketNo();
        m_chatWindows[targetIp]->setPendingFile(packetNo, filePath);
        qDebug() << QString("已暂存待发送文件: %1 (包ID: %2)").arg(filePath).arg(packetNo);
    }
}

void MainWindow::updateUserList()
{
    m_userTreeWidget->clear();
    
    QMap<QString, QTreeWidgetItem*> groups;
    QList<QString> sortedIps = m_users.keys();
    std::sort(sortedIps.begin(), sortedIps.end(), [this](const QString& a, const QString& b) {
        QString groupA = m_users[a].groupName.isEmpty() ? "我的好友" : m_users[a].groupName;
        QString groupB = m_users[b].groupName.isEmpty() ? "我的好友" : m_users[b].groupName;
        if (groupA != groupB) {
            return groupA < groupB;
        }
        QString nameA = m_users[a].displayName.isEmpty() ? m_users[a].sender : m_users[a].displayName;
        QString nameB = m_users[b].displayName.isEmpty() ? m_users[b].sender : m_users[b].displayName;
        return nameA < nameB;
    });
    
    for (const QString& ip : sortedIps) {
        const IPMsg::MessagePacket& userInfo = m_users[ip];
        QString groupName = userInfo.groupName.isEmpty() ? "我的好友" : userInfo.groupName;
        QString displayName = userInfo.displayName.isEmpty() ? userInfo.sender : userInfo.displayName;
        
        if (!groups.contains(groupName)) {
            QTreeWidgetItem* groupItem = new QTreeWidgetItem(m_userTreeWidget);
            groupItem->setText(0, QString("%1 [0]").arg(groupName));
            QMap<QString, QVariant> groupData;
            groupData["type"] = "group";
            groupData["name"] = groupName;
            groupItem->setData(0, Qt::UserRole, groupData);
            groups[groupName] = groupItem;
        }
        
        QTreeWidgetItem* groupItem = groups[groupName];
        QTreeWidgetItem* userItem = new QTreeWidgetItem(groupItem);
        userItem->setText(0, QString("%1 (%2)").arg(displayName).arg(ip));
        QMap<QString, QVariant> userData;
        userData["type"] = "user";
        userData["ip"] = ip;
        userItem->setData(0, Qt::UserRole, userData);
    }
    
    // 更新分组计数
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        int count = it.value()->childCount();
        it.value()->setText(0, QString("%1 [%2]").arg(it.key()).arg(count));
    }
    
    m_userTreeWidget->expandAll();
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
    if (!m_users.contains(targetIp)) {
        qWarning() << QString("目标IP %1 不在当前用户列表中").arg(targetIp);
        return;
    }
    
    if (m_chatWindows.contains(targetIp) && m_chatWindows[targetIp]->isVisible()) {
        m_chatWindows[targetIp]->activateWindow();
        return;
    }
    
    IPMsg::MessagePacket targetUserInfo = m_users[targetIp];
    ChatWindow* chatWin = new ChatWindow(m_username, targetUserInfo, targetIp, 
                                         m_networkManager, this);
    
    // 连接文件传输信号
    connect(chatWin, &ChatWindow::sendFileRequest, 
            this, &MainWindow::onSendFileRequest);
    
    connect(chatWin, &ChatWindow::sendFileReady, 
            [this](quint16 port, quint32 packetNo, const QString& ip) {
                m_networkManager->sendFileReadySignal(port, packetNo, ip);
            });
    
    connect(chatWin, &ChatWindow::destroyed, [this, targetIp]() {
        m_chatWindows.remove(targetIp);
    });
    
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
    
    if (m_networkManager) {
        m_networkManager->stop();
    }
    
    event->accept();
}

