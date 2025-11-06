#include "ui/GroupChatDialog.h"
#include "ui/MainWindow.h"
#include "backend/FeiqBackend.h"
#include <QMessageBox>

GroupChatDialog::GroupChatDialog(const QString& ownUsername,
                                 const QString& groupName,
                                 const QList<QPair<QString, QString>>& recipients,
                                 FeiqBackend* backend,
                                 MainWindow* mainWindow,
                                 QWidget* parent)
    : QDialog(parent)
    , m_ownUsername(ownUsername)
    , m_groupName(groupName)
    , m_recipients(recipients)
    , m_backend(backend)
    , m_mainWindow(mainWindow)
    , m_infoLabel(nullptr)
    , m_messageInput(nullptr)
    , m_sendButton(nullptr)
    , m_cancelButton(nullptr)
{
    setWindowTitle(QString("向组 '%1' 发送消息").arg(groupName));
    setMinimumWidth(400);
    setupUI();
}

void GroupChatDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 接收者信息
    QStringList recipientNames;
    for (const auto& recipient : m_recipients) {
        recipientNames << QString("%1 (%2)").arg(recipient.second).arg(recipient.first);
    }
    
    m_infoLabel = new QLabel(
        QString("<b>接收者 (%1):</b><br>%2")
            .arg(m_recipients.size())
            .arg(recipientNames.join(", ")),
        this
    );
    m_infoLabel->setWordWrap(true);
    mainLayout->addWidget(m_infoLabel);
    
    // 消息输入
    m_messageInput = new QTextEdit(this);
    m_messageInput->setPlaceholderText("在此输入要发送的群组消息...");
    mainLayout->addWidget(m_messageInput);
    
    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_cancelButton = new QPushButton("取消", this);
    m_sendButton = new QPushButton("发送", this);
    m_sendButton->setDefault(true);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_sendButton);
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_sendButton, &QPushButton::clicked, this, &GroupChatDialog::onSendClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &GroupChatDialog::onCancelClicked);
}

void GroupChatDialog::onSendClicked()
{
    QString messageText = m_messageInput->toPlainText().trimmed();
    if (messageText.isEmpty()) {
        QMessageBox::warning(this, "警告", "不能发送空消息！");
        return;
    }
    
    QString fullMessage = QString("(来自群组 '%1' 的消息)\n%2")
        .arg(m_groupName)
        .arg(messageText);
    
    int successCount = 0;
    QStringList failures;
    for (const auto& recipient : m_recipients) {
        QString targetIp = recipient.first;
        
        QString errorMessage;
        if (!m_backend || !m_backend->sendText(targetIp, fullMessage, QString(), &errorMessage)) {
            failures << QString("%1 (%2)").arg(recipient.second, targetIp);
            continue;
        }
        
        // 在本地对应的聊天窗口也显示这条消息
        if (m_mainWindow) {
            m_mainWindow->openChatWindow(targetIp);
            m_mainWindow->appendTextToChat(targetIp, fullMessage, m_ownUsername, true);
        }
        
        successCount++;
    }
    
    if (!failures.isEmpty()) {
        QMessageBox::warning(this,
                             tr("部分发送失败"),
                             tr("以下成员发送失败:\n%1").arg(failures.join("\n")));
    }

    QMessageBox::information(this, "成功", 
        QString("消息已发送给组 '%1' 的 %2 位成员。")
            .arg(m_groupName)
            .arg(successCount));
    
    accept();
}

void GroupChatDialog::onCancelClicked()
{
    reject();
}

