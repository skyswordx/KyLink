#include "ScreenshotTool.h"
#include <QScreen>
#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QWindow>
#include <QProcess>
#include <QFile>

ScreenshotTool::ScreenshotTool(QWidget* parent)
    : QWidget(parent)
    , m_isSelecting(false)
    , m_screenCaptured(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setCursor(Qt::CrossCursor);
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    // 在显示窗口之前先截图
    captureScreenBeforeShow();
    
    // 获取屏幕尺寸
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect screenGeometry;
    if (screen) {
        screenGeometry = screen->geometry();
    } else {
        screenGeometry = QRect(0, 0, 1920, 1080);
    }
    
    setGeometry(screenGeometry);
    
    // 设置窗口背景色为浅灰色，避免黑屏
    setStyleSheet("background-color: #f0f0f0;");
    
    raise();
    activateWindow();
    setFocus();
}

void ScreenshotTool::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    
    // 如果截图还未完成，延迟一下再显示
    if (!m_screenCaptured && m_screenPixmap.isNull()) {
        QTimer::singleShot(100, this, &ScreenshotTool::onDelayedCapture);
    }
}

void ScreenshotTool::onDelayedCapture()
{
    if (m_screenPixmap.isNull()) {
        captureScreenBeforeShow();
        update();
    }
}

void ScreenshotTool::captureScreenBeforeShow()
{
    // 不隐藏自己，因为此时窗口还未显示
    // 直接截图整个屏幕
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        qWarning() << "无法获取屏幕";
        return;
    }
    
    // 方法1: 尝试使用QScreen::grabWindow()截图整个屏幕
    // 在Linux/X11环境下，可以使用root window ID (0)
    m_screenPixmap = screen->grabWindow(0);
    
    // 如果失败，尝试其他方法
    if (m_screenPixmap.isNull()) {
        // 方法2: 尝试获取桌面窗口ID
        QWidget* desktop = QApplication::desktop();
        if (desktop) {
            m_screenPixmap = screen->grabWindow(desktop->winId());
        }
    }
    
    // 如果还是失败，尝试使用QScreen直接截图
    if (m_screenPixmap.isNull()) {
        QRect geometry = screen->geometry();
        QWidget* desktop = QApplication::desktop();
        if (desktop) {
            m_screenPixmap = screen->grabWindow(desktop->winId(), 
                                                geometry.x(), geometry.y(), 
                                                geometry.width(), geometry.height());
        }
    }
    
    // 方法4: 尝试使用import命令（ImageMagick）作为备用方案
    if (m_screenPixmap.isNull()) {
        QRect geometry = screen->geometry();
        QString tempFile = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                          "/screenshot_temp_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
        
        QProcess process;
        process.start("import", QStringList() << "-window" << "root" << tempFile);
        if (process.waitForFinished(2000) && QFile::exists(tempFile)) {
            m_screenPixmap.load(tempFile);
            QFile::remove(tempFile);
        }
    }
    
    // 最后的备用方案：创建一个半透明的占位图
    if (m_screenPixmap.isNull()) {
        QRect geometry = screen->geometry();
        m_screenPixmap = QPixmap(geometry.size());
        m_screenPixmap.fill(QColor(240, 240, 240)); // 浅灰色背景，而不是黑色
        qWarning() << "无法获取屏幕截图，使用占位图。建议安装ImageMagick (import命令)或使用系统截图工具";
    } else {
        m_screenCaptured = true;
        qDebug() << "屏幕截图成功，尺寸:" << m_screenPixmap.size();
    }
}

void ScreenshotTool::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    
    // 如果还没有截图，显示提示信息而不是黑屏
    if (m_screenPixmap.isNull()) {
        painter.fillRect(rect(), QColor(240, 240, 240)); // 浅灰色背景
        painter.setPen(QPen(Qt::black));
        painter.setFont(QFont("Arial", 16));
        QString message = "无法获取屏幕截图\n\n"
                         "在WSL环境中，Qt截图功能可能不可用\n"
                         "请使用系统截图工具（如Windows截图工具）\n"
                         "或安装ImageMagick: sudo apt-get install imagemagick\n\n"
                         "按ESC退出";
        painter.drawText(rect(), Qt::AlignCenter, message);
        return;
    }
    
    // 绘制屏幕截图
    painter.drawPixmap(rect(), m_screenPixmap);
    
    // 绘制半透明遮罩
    painter.fillRect(rect(), QColor(0, 0, 0, 128));
    
    // 如果有选择区域，显示选中部分
    if (m_isSelecting && !m_startPoint.isNull() && !m_endPoint.isNull()) {
        QRect selectionRect = getSelectionRect();
        
        // 显示选中区域（不遮罩）
        QPixmap selectedPixmap = m_screenPixmap.copy(selectionRect);
        painter.drawPixmap(selectionRect.topLeft(), selectedPixmap);
        
        // 绘制选择框
        painter.setPen(QPen(Qt::red, 2, Qt::SolidLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(selectionRect);
    }
}

void ScreenshotTool::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_endPoint = m_startPoint;
        m_isSelecting = true;
        update();
    }
}

void ScreenshotTool::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isSelecting) {
        m_endPoint = event->pos();
        update();
    }
}

void ScreenshotTool::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_endPoint = event->pos();
        m_isSelecting = false;
        captureScreenshot();
        close();
    }
}

void ScreenshotTool::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void ScreenshotTool::captureScreenshot()
{
    if (m_startPoint.isNull() || m_endPoint.isNull()) {
        return;
    }
    
    QRect selectionRect = getSelectionRect();
    if (selectionRect.isEmpty()) {
        return;
    }
    
    QPixmap screenshot = m_screenPixmap.copy(selectionRect);
    
    // 确保缓存目录存在
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(cacheDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 生成文件名
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    QString filename = QString("screenshot_%1.png").arg(timestamp);
    QString filePath = QDir(cacheDir).filePath(filename);
    
    // 保存截图
    if (screenshot.save(filePath, "PNG")) {
        qDebug() << "截图已保存到:" << filePath;
        emit screenshotTaken(filePath);
    } else {
        qWarning() << "截图保存失败:" << filePath;
    }
}

QRect ScreenshotTool::getSelectionRect() const
{
    QRect rect(m_startPoint, m_endPoint);
    return rect.normalized();
}

