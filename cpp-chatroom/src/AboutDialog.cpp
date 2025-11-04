#include "AboutDialog.h"
#include <QTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QCoreApplication>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("关于 FeiQ Chatroom");
    setMinimumWidth(500);
    setMinimumHeight(400);
    setupUI();
}

void AboutDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 标题
    QLabel* titleLabel = createTitleLabel("FeiQ Chatroom");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // 版本信息
    const QString version = QCoreApplication::applicationVersion().isEmpty()
        ? QStringLiteral("1.0.1")
        : QCoreApplication::applicationVersion();
    QLabel* versionLabel = new QLabel(QStringLiteral("版本 %1").arg(version), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    QFont versionFont = versionLabel->font();
    versionFont.setPointSize(10);
    versionLabel->setFont(versionFont);
    mainLayout->addWidget(versionLabel);
    
    // 分隔线
    QLabel* separator = new QLabel("─────────────────────", this);
    separator->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(separator);
    
    // 信息文本
    QTextEdit* infoText = createInfoTextEdit();
    mainLayout->addWidget(infoText);
    
    // 按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton("关闭", this);
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);
    
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

QLabel* AboutDialog::createTitleLabel(const QString& text)
{
    QLabel* label = new QLabel(text, this);
    QFont font = label->font();
    font.setPointSize(18);
    font.setBold(true);
    label->setFont(font);
    return label;
}

QTextEdit* AboutDialog::createInfoTextEdit()
{
    QTextEdit* textEdit = new QTextEdit(this);
    textEdit->setReadOnly(true);
    
    QString info = QString(
        "<h3>作者信息</h3>"
        "<p>基于 Qt5 和 C++ 实现的飞秋（FeiQ）聊天室应用程序</p>"
        "<p>使用 IPMsg 协议进行通信</p>"
        
        "<h3>软件功能</h3>"
        "<ul>"
        "<li>✓ UDP 广播通信（上线/下线/消息）</li>"
        "<li>✓ 用户列表发现和维护</li>"
        "<li>✓ 点对点聊天</li>"
        "<li>✓ 群发消息</li>"
        "<li>✓ TCP 文件传输（发送和接收）</li>"
        "<li>✓ 图片发送和接收</li>"
        "<li>✓ 截图功能</li>"
        "<li>✓ 多线程文件传输（不阻塞 GUI）</li>"
        "<li>✓ 配置文件管理</li>"
        "</ul>"
        
        "<h3>软件特性</h3>"
        "<ul>"
        "<li>✓ 高性能 C++ 实现</li>"
        "<li>✓ 跨平台支持（Ubuntu、Windows、嵌入式 Linux）</li>"
        "<li>✓ 兼容飞秋协议格式</li>"
        "<li>✓ 现代化 Qt5 界面</li>"
        "<li>✓ 健壮的错误处理机制</li>"
        "<li>✓ 实时用户状态更新</li>"
        "<li>✓ 文件传输进度支持</li>"
        "</ul>"
        
        "<h3>未来计划</h3>"
        "<ul>"
        "<li>• 实现文件传输进度条显示</li>"
        "<li>• 添加消息加密功能</li>"
        "<li>• 支持自定义表情包</li>"
        "<li>• 实现消息历史记录</li>"
        "<li>• 添加语音消息支持</li>"
        "<li>• 支持文件传输暂停/恢复</li>"
        "<li>• 优化 UI 样式和主题</li>"
        "<li>• 添加多语言支持</li>"
        "</ul>"
        
        "<h3>技术栈</h3>"
        "<p>Qt5, C++17, CMake</p>"
        "<p>协议: IPMsg/飞秋协议</p>"
    );
    
    textEdit->setHtml(info);
    return textEdit;
}

