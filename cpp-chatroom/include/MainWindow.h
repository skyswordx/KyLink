#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QMap>
#include <QString>
#include <QVariant>
#include "NetworkManager.h"
#include "protocol.h"

class ChatWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void handleUserOnline(const IPMsg::MessagePacket& msg, const QString& ip);
    void handleUserOffline(const IPMsg::MessagePacket& msg, const QString& ip);
    void handleMessageReceived(const IPMsg::MessagePacket& msg, const QString& ip);
    void handleFileRequestReceived(const IPMsg::MessagePacket& msg, const QString& ip);
    void handleFileReceiverReady(const IPMsg::MessagePacket& msg, const QString& ip);
    
    void onUserItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onSearchTextChanged(const QString& text);
    void onGroupMessageClicked();
    
    void onSendFileRequest(const QString& targetIp, const QString& filePath);

private:
    void setupUI();
    void setupConnections();
    void updateUserList();
    void filterUserList();
    void openChatWindow(const QString& targetIp);
    
    // UI组件
    QTreeWidget* m_userTreeWidget;
    QLineEdit* m_searchInput;
    QLabel* m_statusLabel;
    QPushButton* m_groupMessageButton;
    
    // 数据
    QString m_username;
    QString m_hostname;
    QString m_groupname;
    
    NetworkManager* m_networkManager;
    QMap<QString, IPMsg::MessagePacket> m_users;  // IP -> UserInfo
    QMap<QString, ChatWindow*> m_chatWindows;      // IP -> ChatWindow
};

#endif // MAINWINDOW_H

