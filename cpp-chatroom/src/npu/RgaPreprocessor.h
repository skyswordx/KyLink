#pragma once

#include <QByteArray>

#include "RgaPreprocessor.h"

class RgaPreprocessor {
public:
    enum class Result {
        Success,
        Failure,
        // ... other possible results
    };

    struct Nv12Input {
        const uint8_t* yPlane;
        const uint8_t* uvPlane;
        int width;
        int height;
        int yStride;
        int uvStride;
    };

    RgaPreprocessor(int targetWidth, int targetHeight)
        : targetWidth_(targetWidth)
        , targetHeight_(targetHeight) {}

    Result processNv12(const Nv12Input& input);

    // 新增：专门用于生成显示用的图像（RGA 缩放 + 格式转换）
    Result processNv12ToDisplay(const Nv12Input& input, int displayWidth, int displayHeight);

    int targetWidth() const { return targetWidth_; }
    int targetHeight() const { return targetHeight_; }

private:
    int targetWidth_;
    int targetHeight_;
    QByteArray outputBuffer_;
    QByteArray displayBuffer_; // 新增：显示用的缓冲
};