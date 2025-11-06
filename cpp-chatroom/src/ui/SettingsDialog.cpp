#include "ui/SettingsDialog.h"
#include <QSettings>
#include <QMessageBox>
#include <QDebug>
#include <QHostInfo>
#include <QGroupBox>
#include <QFormLayout>
#include "ipmsg.h"

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
    , m_usernameEdit(nullptr)
    , m_hostnameEdit(nullptr)
    , m_groupnameEdit(nullptr)
    , m_portSpinBox(nullptr)
    , m_compatibleModeCheck(nullptr)
    , m_autoStartCheck(nullptr)
    , m_showNotificationsCheck(nullptr)
    , m_saveButton(nullptr)
    , m_cancelButton(nullptr)
    , m_resetButton(nullptr)
{
    setWindowTitle("设置");
    setMinimumWidth(400);
    setupUI();
    loadSettings();
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 基本信息组
    QGroupBox* basicGroup = new QGroupBox("基本信息", this);
    QFormLayout* basicLayout = new QFormLayout(basicGroup);
    
    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText("输入用户名");
    basicLayout->addRow("用户名:", m_usernameEdit);
    
    m_hostnameEdit = new QLineEdit(this);
    m_hostnameEdit->setPlaceholderText("输入主机名");
    basicLayout->addRow("主机名:", m_hostnameEdit);
    
    m_groupnameEdit = new QLineEdit(this);
    m_groupnameEdit->setPlaceholderText("输入组名");
    basicLayout->addRow("组名:", m_groupnameEdit);
    
    // 网络设置组
    QGroupBox* networkGroup = new QGroupBox("网络设置", this);
    QFormLayout* networkLayout = new QFormLayout(networkGroup);
    
    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setRange(1024, 65535);
    m_portSpinBox->setValue(IPMSG_PORT);
    m_portSpinBox->setToolTip("UDP通信端口，默认2425");
    networkLayout->addRow("端口:", m_portSpinBox);
    
    m_compatibleModeCheck = new QCheckBox("兼容飞秋格式", this);
    m_compatibleModeCheck->setToolTip("启用后与飞秋客户端完全兼容");
    m_compatibleModeCheck->setChecked(true);
    networkLayout->addRow(m_compatibleModeCheck);
    
    // 其他设置组
    QGroupBox* otherGroup = new QGroupBox("其他设置", this);
    QVBoxLayout* otherLayout = new QVBoxLayout(otherGroup);
    
    m_autoStartCheck = new QCheckBox("开机自动启动", this);
    m_showNotificationsCheck = new QCheckBox("显示通知", this);
    m_showNotificationsCheck->setChecked(true);
    
    otherLayout->addWidget(m_autoStartCheck);
    otherLayout->addWidget(m_showNotificationsCheck);
    
    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_resetButton = new QPushButton("重置", this);
    m_resetButton->setToolTip("恢复默认设置");
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addStretch();
    m_cancelButton = new QPushButton("取消", this);
    m_saveButton = new QPushButton("保存", this);
    m_saveButton->setDefault(true);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_saveButton);
    
    // 添加到主布局
    mainLayout->addWidget(basicGroup);
    mainLayout->addWidget(networkGroup);
    mainLayout->addWidget(otherGroup);
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_saveButton, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancelClicked);
    connect(m_resetButton, &QPushButton::clicked, this, &SettingsDialog::onResetClicked);
}

void SettingsDialog::loadSettings()
{
    QSettings settings("FeiQChatroom", "Settings");
    
    m_usernameEdit->setText(settings.value("username", "CppUser").toString());
    m_hostnameEdit->setText(settings.value("hostname", "").toString());
    m_groupnameEdit->setText(settings.value("groupname", "我的好友").toString());
    m_portSpinBox->setValue(settings.value("port", IPMSG_PORT).toInt());
    m_compatibleModeCheck->setChecked(settings.value("compatibleMode", true).toBool());
    m_autoStartCheck->setChecked(settings.value("autoStart", false).toBool());
    m_showNotificationsCheck->setChecked(settings.value("showNotifications", true).toBool());
}

void SettingsDialog::saveSettings()
{
    QSettings settings("FeiQChatroom", "Settings");
    
    settings.setValue("username", m_usernameEdit->text());
    settings.setValue("hostname", m_hostnameEdit->text());
    settings.setValue("groupname", m_groupnameEdit->text());
    settings.setValue("port", m_portSpinBox->value());
    settings.setValue("compatibleMode", m_compatibleModeCheck->isChecked());
    settings.setValue("autoStart", m_autoStartCheck->isChecked());
    settings.setValue("showNotifications", m_showNotificationsCheck->isChecked());
    
    settings.sync();
}

void SettingsDialog::onSaveClicked()
{
    if (m_usernameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "警告", "用户名不能为空！");
        m_usernameEdit->setFocus();
        return;
    }
    
    if (m_groupnameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "警告", "组名不能为空！");
        m_groupnameEdit->setFocus();
        return;
    }
    
    saveSettings();
    emit settingsChanged();
    accept();
}

void SettingsDialog::onCancelClicked()
{
    reject();
}

void SettingsDialog::onResetClicked()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "确认", "确定要重置所有设置为默认值吗？",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_usernameEdit->setText("CppUser");
        m_hostnameEdit->setText("");
        m_groupnameEdit->setText("我的好友");
        m_portSpinBox->setValue(IPMSG_PORT);
        m_compatibleModeCheck->setChecked(true);
        m_autoStartCheck->setChecked(false);
        m_showNotificationsCheck->setChecked(true);
    }
}

QString SettingsDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString SettingsDialog::hostname() const
{
    QString hostname = m_hostnameEdit->text().trimmed();
    return hostname.isEmpty() ? QHostInfo::localHostName() : hostname;
}

QString SettingsDialog::groupname() const
{
    return m_groupnameEdit->text().trimmed();
}

quint16 SettingsDialog::port() const
{
    return static_cast<quint16>(m_portSpinBox->value());
}

bool SettingsDialog::compatibleMode() const
{
    return m_compatibleModeCheck->isChecked();
}

bool SettingsDialog::autoStart() const
{
    return m_autoStartCheck->isChecked();
}

bool SettingsDialog::showNotifications() const
{
    return m_showNotificationsCheck->isChecked();
}

void SettingsDialog::setUsername(const QString& username)
{
    m_usernameEdit->setText(username);
}

void SettingsDialog::setHostname(const QString& hostname)
{
    m_hostnameEdit->setText(hostname);
}

void SettingsDialog::setGroupname(const QString& groupname)
{
    m_groupnameEdit->setText(groupname);
}

void SettingsDialog::setPort(quint16 port)
{
    m_portSpinBox->setValue(port);
}

void SettingsDialog::setCompatibleMode(bool enabled)
{
    m_compatibleModeCheck->setChecked(enabled);
}

void SettingsDialog::setAutoStart(bool enabled)
{
    m_autoStartCheck->setChecked(enabled);
}

void SettingsDialog::setShowNotifications(bool enabled)
{
    m_showNotificationsCheck->setChecked(enabled);
}
