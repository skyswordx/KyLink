#ifndef RK3566_CHATROOM_RGA_PREPROCESSOR_H
#define RK3566_CHATROOM_RGA_PREPROCESSOR_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <cstdint>

#include "npu/InferenceGeometry.h"

namespace cv {
class Mat;
}

class RgaPreprocessor {
public:
    struct Result {
        bool success = false;
        const uint8_t* data = nullptr;
        int width = 0;
        int height = 0;
        int stride = 0;
        qint64 durationUs = -1;
        bool usedRga = false;
        QString error;
        LetterboxTransform letterbox;
    };

    struct Nv12Input {
        const uint8_t* data = nullptr;
        int width = 0;
        int height = 0;
        int yStride = 0;
        int uvStride = 0;
        int yPlaneOffset = 0;
        int uvPlaneOffset = 0;
        int dmaFd = -1;
        uint8_t* targetVirtual = nullptr;
        int targetStride = 0;
        int targetDmaFd = -1;
    };

    bool initialize(int targetWidth, int targetHeight);
    Result processBgr(const cv::Mat& frame);
    Result processNv12(const Nv12Input& input);

    // 新增：专门用于生成显示用的图像（RGA 缩放 + 格式转换）
    Result processNv12ToDisplay(const Nv12Input& input, int displayWidth, int displayHeight);

    int targetWidth() const { return targetWidth_; }
    int targetHeight() const { return targetHeight_; }

private:
    Result processWithRga(const cv::Mat& frame);
    Result processWithCpu(const cv::Mat& frame);
    Result processNv12WithRga(const Nv12Input& input);
    Result processNv12Cpu(const Nv12Input& input);
    LetterboxTransform computeLetterbox(int srcWidth, int srcHeight) const;
    uint8_t* resolveOutputBuffer(uint8_t* preferred, int preferredStride, int& actualStrideBytes);
    void clearTargetBuffer(uint8_t* target, int strideBytes);
    void blitIntoLetterbox(const LetterboxTransform& letterbox,
                           uint8_t* target,
                           int targetStrideBytes,
                           const uint8_t* src,
                           int srcStrideBytes);
    void ensureBuffers();
    std::size_t targetRgbBytes() const;
    std::size_t targetNv12Bytes() const;

    bool initialized_ = false;
    int targetWidth_ = 0;
    int targetHeight_ = 0;
    QByteArray outputBuffer_;
    QByteArray displayBuffer_; // 新增：显示用的缓冲
};

#endif // RK3566_CHATROOM_RGA_PREPROCESSOR_H
