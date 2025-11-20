#ifndef RK3566_CHATROOM_RGA_PREPROCESSOR_H
#define RK3566_CHATROOM_RGA_PREPROCESSOR_H

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <cstdint>

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
    };

    bool initialize(int targetWidth, int targetHeight);
    Result processBgr(const cv::Mat& frame);
    Result processNv12(const Nv12Input& input);

    int targetWidth() const { return targetWidth_; }
    int targetHeight() const { return targetHeight_; }

private:
    Result processWithRga(const cv::Mat& frame);
    Result processWithCpu(const cv::Mat& frame);
    Result processNv12WithRga(const Nv12Input& input);
    Result processNv12Cpu(const Nv12Input& input);
    void ensureBuffers();
    std::size_t targetRgbBytes() const;
    std::size_t targetNv12Bytes() const;

    bool initialized_ = false;
    int targetWidth_ = 0;
    int targetHeight_ = 0;
    QByteArray outputBuffer_;
    QByteArray resizeScratchBuffer_;
    QByteArray nv12ScratchBuffer_;
};

#endif // RK3566_CHATROOM_RGA_PREPROCESSOR_H
