#include "ui/ScreenshotTool.h"
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
#include <QStringList>
#include <QObject>

namespace {

bool captureWithQScreen(QScreen* screen, QPixmap& outPixmap)
{
    if (!screen) {
        return false;
    }

    QPixmap pixmap = screen->grabWindow(0);
    if (!pixmap.isNull()) {
        outPixmap = pixmap;
        return true;
    }

    QWidget* desktop = QApplication::desktop();
    if (!desktop) {
        return false;
    }

    pixmap = screen->grabWindow(desktop->winId());
    if (!pixmap.isNull()) {
        outPixmap = pixmap;
        return true;
    }

    const QRect geometry = screen->geometry();
    pixmap = screen->grabWindow(desktop->winId(), geometry.x(), geometry.y(), geometry.width(), geometry.height());
    if (!pixmap.isNull()) {
        outPixmap = pixmap;
        return true;
    }

    return false;
}

QString createTempScreenshotPath(const QString& prefix)
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseDir.isEmpty()) {
        baseDir = QDir::tempPath();
    }

    QDir dir(baseDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    return dir.filePath(QStringLiteral("%1_%2.png").arg(prefix).arg(QDateTime::currentMSecsSinceEpoch()));
}

bool loadPixmapAndCleanup(const QString& path, QPixmap& outPixmap, QString* errorMessage)
{
    if (!QFile::exists(path)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("截图临时文件不存在: %1").arg(path);
        }
        return false;
    }

    QPixmap pixmap;
    if (!pixmap.load(path)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("无法加载截图临时文件: %1").arg(path);
        }
        QFile::remove(path);
        return false;
    }

    QFile::remove(path);
    outPixmap = pixmap;
    return true;
}

bool captureWithExternalProgram(const QString& program,
                                const QStringList& arguments,
                                const QString& tempPath,
                                QPixmap& outPixmap,
                                QString* errorMessage)
{
    if (QStandardPaths::findExecutable(program).isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("未找到外部截图工具 %1").arg(program);
        }
        return false;
    }

    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(8000)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("%1 执行超时").arg(program);
        }
        process.kill();
        QFile::remove(tempPath);
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            const QString stderrOutput = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorMessage = QObject::tr("%1 执行失败: %2").arg(program, stderrOutput);
        }
        QFile::remove(tempPath);
        return false;
    }

    return loadPixmapAndCleanup(tempPath, outPixmap, errorMessage);
}

bool isWaylandSession()
{
    const QString sessionType = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"));
    if (sessionType.compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0) {
        return true;
    }

    const QString platform = QGuiApplication::platformName();
    return platform.contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
}

#ifdef Q_OS_LINUX
bool isRunningInWSL()
{
    if (qEnvironmentVariableIsSet("WSL_INTEROP") || qEnvironmentVariableIsSet("WSL_DISTRO_NAME")) {
        return true;
    }

    QFile releaseFile(QStringLiteral("/proc/sys/kernel/osrelease"));
    if (releaseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString content = QString::fromUtf8(releaseFile.readAll());
        if (content.contains(QStringLiteral("Microsoft"), Qt::CaseInsensitive) ||
            content.contains(QStringLiteral("WSL"), Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

QString toWindowsPath(const QString& wslPath)
{
    QProcess process;
    process.start(QStringLiteral("wslpath"), {QStringLiteral("-w"), wslPath});
    if (!process.waitForFinished(2000)) {
        process.kill();
        return QString();
    }

    return QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
}

bool captureWithPowerShell(const QString& tempPath, QPixmap& outPixmap, QString* errorMessage)
{
    const QString windowsPath = toWindowsPath(tempPath);
    if (windowsPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("wslpath 转换路径失败");
        }
        return false;
    }

    QString scriptPath = windowsPath;
    scriptPath.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    scriptPath.replace(QStringLiteral("\""), QStringLiteral("\\\""));

    const QString script = QStringLiteral(
        "$path=\"%1\";"
        "Add-Type -AssemblyName System.Windows.Forms;"
        "Add-Type -AssemblyName System.Drawing;"
        "$bounds=[System.Windows.Forms.Screen]::PrimaryScreen.Bounds;"
        "$bmp=New-Object System.Drawing.Bitmap($bounds.Width,$bounds.Height);"
        "$gfx=[System.Drawing.Graphics]::FromImage($bmp);"
        "$gfx.CopyFromScreen($bounds.Location,[System.Drawing.Point]::Empty,$bounds.Size);"
        "$bmp.Save($path,[System.Drawing.Imaging.ImageFormat]::Png);"
        "$gfx.Dispose();$bmp.Dispose();"
    ).arg(scriptPath);

    QProcess process;
    process.start(QStringLiteral("powershell.exe"), {QStringLiteral("-NoProfile"), QStringLiteral("-Command"), script});
    if (!process.waitForFinished(10000)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("PowerShell 截图超时");
        }
        process.kill();
        QFile::remove(tempPath);
        return false;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage) {
            const QString stderrOutput = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *errorMessage = QObject::tr("PowerShell 截图失败: %1").arg(stderrOutput);
        }
        QFile::remove(tempPath);
        return false;
    }

    return loadPixmapAndCleanup(tempPath, outPixmap, errorMessage);
}
#endif // Q_OS_LINUX

} // namespace

ScreenshotTool::ScreenshotTool(QWidget* parent)
    : QWidget(parent)
    , m_isSelecting(false)
    , m_screenCaptured(false)
    , m_lastError()
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
    
    m_lastError.clear();

    QStringList failureReasons;

    bool captured = captureWithQScreen(screen, m_screenPixmap);
    if (!captured) {
        failureReasons << tr("Qt 内置抓屏失败（可能受到会话限制）");
    }

#ifdef Q_OS_LINUX
    const bool runningInWSL = isRunningInWSL();
#else
    const bool runningInWSL = false;
#endif
    const bool runningOnWayland = isWaylandSession();

#ifdef Q_OS_LINUX
    if (!captured && runningInWSL) {
        QString errorMessage;
        const QString tempPath = createTempScreenshotPath(QStringLiteral("feiQ_wsl"));
        captured = captureWithPowerShell(tempPath, m_screenPixmap, &errorMessage);
        if (!captured) {
            failureReasons << (errorMessage.isEmpty() ? tr("PowerShell 截图失败") : errorMessage);
        }
    }
#endif

    if (!captured && runningOnWayland) {
        QString errorMessage;
        const QString tempPath = createTempScreenshotPath(QStringLiteral("feiQ_wayland"));
        captured = captureWithExternalProgram(QStringLiteral("gnome-screenshot"),
                                              {QStringLiteral("--file"), tempPath},
                                              tempPath,
                                              m_screenPixmap,
                                              &errorMessage);
        if (!captured) {
            failureReasons << (errorMessage.isEmpty() ? tr("gnome-screenshot 截图失败") : errorMessage);
        }
    }

    if (!captured) {
        QString errorMessage;
        const QString tempPath = createTempScreenshotPath(QStringLiteral("feiQ_import"));
        captured = captureWithExternalProgram(QStringLiteral("import"),
                                              {QStringLiteral("-window"), QStringLiteral("root"), tempPath},
                                              tempPath,
                                              m_screenPixmap,
                                              &errorMessage);
        if (!captured) {
            failureReasons << (errorMessage.isEmpty() ? tr("ImageMagick import 截图失败") : errorMessage);
        }
    }

    if (!captured) {
        const QRect geometry = screen->geometry();
        m_screenPixmap = QPixmap(geometry.size());
        m_screenPixmap.fill(QColor(240, 240, 240));
        m_screenCaptured = false;

        if (failureReasons.isEmpty()) {
            failureReasons << tr("无法获取屏幕截图，请检查图形会话或外部截图工具");
        }

        if (runningInWSL) {
            failureReasons << tr("当前运行于 WSL 环境，建议在 Windows 原生版本中使用截图功能");
        }

        m_lastError = failureReasons.join(QStringLiteral("\n"));
        qWarning() << "无法获取屏幕截图:" << m_lastError;
    } else {
        m_screenCaptured = true;
        m_lastError.clear();
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
        QString message = m_lastError;
        if (message.isEmpty()) {
            message = tr("无法获取屏幕截图\n\n请检查图形会话或安装外部截图工具（gnome-screenshot / imagemagick）\n\n按ESC退出");
        } else {
            message.append(QStringLiteral("\n\n按ESC退出"));
        }
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

