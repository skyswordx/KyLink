#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAction>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QTreeWidget>
#include <QVariant>
#include "backend/FeiqBackend.h"
#include "domain/FeiqTypes.h"

class ChatWindow;
class SettingsDialog;
class AboutDialog;
class GroupChatDialog;
#ifdef BUILD_RK3566
class PerformanceAnalyticsDialog;
#endif

class MainWindow : public QMainWindow
{
    Q_OBJECT

    friend class GroupChatDialog;

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

public slots:
    void appendTextToChat(const QString& targetIp, const QString& message,
                          const QString& senderName, bool isOwn);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void handleFellowUpdated(const FeiqFellowInfo& fellow);
    void handleMessageReceived(const FeiqMessage& message);
    void handleSendTimeout(const FeiqFellowInfo& fellow, const QString& description);
    void handleFileTaskUpdated(const FeiqFileTaskInfo& info);
    
    void onUserItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onUserItemActivated(QTreeWidgetItem* item, int column);
    void onSearchTextChanged(const QString& text);
    void onGroupMessageClicked();
    void onSendFileRequest(const QString& targetIp, const QString& filePath);
    void onSettingsClicked();
    void onAboutClicked();
#ifdef BUILD_RK3566
    void onOpenPerformanceAnalytics();
#endif
#ifdef BUILD_DESKTOP
    void onOpenVideoPlayer();
#endif
    
    // 供GroupChatDialog调用的公共方法
    void appendFileOfferToChat(const QString& targetIp, const FeiqFileOffer& offer,
                               const QString& senderName);
    void promptFileDownload(const QString& targetIp, const FeiqFileOffer& offer,
                            const QString& senderName);
    void refreshUserNode(const FeiqFellowInfo& fellow);
    void ensureChatWindowSignals(ChatWindow* chatWindow);

private:
    void setupUI();
    void setupMenuBar();
    void setupConnections();
    void updateUserList();
    void filterUserList();
    void openChatWindow(const QString& targetIp);
    void loadSettings();
    
    // UI组件
    QTreeWidget* m_userTreeWidget;
    QLineEdit* m_searchInput;
    QLabel* m_statusLabel;
    QPushButton* m_groupMessageButton;
    
    // 菜单
    QMenu* m_fileMenu;
    QMenu* m_helpMenu;
#ifdef BUILD_RK3566
    QMenu* m_performanceMenu;
    QAction* m_openPerformanceAction;
#endif
#ifdef BUILD_DESKTOP
    QMenu* m_videoMenu;
    QAction* m_openVideoPlayerAction;
#endif
    QAction* m_settingsAction;
    QAction* m_exitAction;
    QAction* m_aboutAction;
    
    // 数据
    QString m_username;
    QString m_hostname;
    QString m_groupname;
    
    FeiqBackend* m_backend;
    QMap<QString, FeiqFellowInfo> m_users;  // IP -> UserInfo
    QMap<QString, ChatWindow*> m_chatWindows;      // IP -> ChatWindow
#ifdef BUILD_RK3566
    QPointer<PerformanceAnalyticsDialog> m_performanceDialog;
#endif
#ifdef BUILD_DESKTOP
    QPointer<class VideoPlayerDialog> m_videoPlayerDialog;
#endif
};

#endif // MAINWINDOW_H

