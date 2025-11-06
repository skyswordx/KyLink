#ifndef GROUPCHATDIALOG_H
#define GROUPCHATDIALOG_H

#include <QDialog>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "domain/FeiqTypes.h"

class FeiqBackend;
class MainWindow;

class GroupChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GroupChatDialog(const QString& ownUsername,
                           const QString& groupName,
                           const QList<QPair<QString, QString>>& recipients, // IP -> Name
                           FeiqBackend* backend,
                           MainWindow* mainWindow,
                           QWidget* parent = nullptr);

private slots:
    void onSendClicked();
    void onCancelClicked();

private:
    void setupUI();
    
    QString m_ownUsername;
    QString m_groupName;
    QList<QPair<QString, QString>> m_recipients; // IP -> Name
    FeiqBackend* m_backend;
    MainWindow* m_mainWindow;
    
    QLabel* m_infoLabel;
    QTextEdit* m_messageInput;
    QPushButton* m_sendButton;
    QPushButton* m_cancelButton;
};

#endif // GROUPCHATDIALOG_H

