#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setApplicationName(QStringLiteral("FeiQ Chatroom"));
#ifdef PROJECT_VERSION_STR
    QCoreApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION_STR));
#endif
    
    // 确保缓存目录存在
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir dir(cacheDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}

