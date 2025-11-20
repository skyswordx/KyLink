#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

#include "backend/RgaSelfTest.h"
#include "npu/YoloV5Runner.h"
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
    
    // 启动阶段执行一次 NPU 示例推理，验证驱动链路
    const QString assetDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("npu_assets"));
    const QString modelPath = QDir(assetDir).filePath(QStringLiteral("yolov5s_relu.rknn"));
    const QString imagePath = QDir(assetDir).filePath(QStringLiteral("busstop.jpg"));
    const QString outputPath = QDir(cacheDir).filePath(QStringLiteral("npu_busstop_output.jpg"));

    QString npuError;
    YoloV5Runner runner;
    if (!runner.runSample(modelPath, imagePath, outputPath, &npuError)) {
        qWarning().noquote() << QStringLiteral("NPU 示例推理失败: %1").arg(npuError);
    }

    QString rgaError;
    if (!diagnostics::runRgaSelfTest(&rgaError)) {
        qWarning().noquote() << QStringLiteral("RGA 自检失败: %1").arg(rgaError);
    }

    // 创建并显示主窗口
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}

