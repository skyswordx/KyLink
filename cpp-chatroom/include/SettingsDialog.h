#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    ~SettingsDialog();

    // 获取配置
    QString username() const;
    QString hostname() const;
    QString groupname() const;
    quint16 port() const;
    bool compatibleMode() const;
    bool autoStart() const;
    bool showNotifications() const;

    // 设置配置
    void setUsername(const QString& username);
    void setHostname(const QString& hostname);
    void setGroupname(const QString& groupname);
    void setPort(quint16 port);
    void setCompatibleMode(bool enabled);
    void setAutoStart(bool enabled);
    void setShowNotifications(bool enabled);

signals:
    void settingsChanged();

private slots:
    void onSaveClicked();
    void onCancelClicked();
    void onResetClicked();

private:
    void setupUI();
    void loadSettings();
    void saveSettings();

    // 基本信息
    QLineEdit* m_usernameEdit;
    QLineEdit* m_hostnameEdit;
    QLineEdit* m_groupnameEdit;
    
    // 网络设置
    QSpinBox* m_portSpinBox;
    QCheckBox* m_compatibleModeCheck;
    
    // 其他设置
    QCheckBox* m_autoStartCheck;
    QCheckBox* m_showNotificationsCheck;
    
    // 按钮
    QPushButton* m_saveButton;
    QPushButton* m_cancelButton;
    QPushButton* m_resetButton;
};

#endif // SETTINGSDIALOG_H

