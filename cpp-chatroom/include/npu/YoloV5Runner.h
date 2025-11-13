#ifndef RK3566_CHATROOM_YOLOV5RUNNER_H
#define RK3566_CHATROOM_YOLOV5RUNNER_H

#include <QString>

class YoloV5Runner {
public:
    bool runSample(const QString &modelPath,
                   const QString &inputImagePath,
                   const QString &outputImagePath,
                   QString *errorOut) const;
};

#endif
