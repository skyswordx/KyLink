#include "npu/RgaPreprocessor.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QLoggingCategory>

#include <opencv2/imgproc.hpp>

#ifdef HAVE_RGA
#include <rga/im2d.hpp>
#include <rga/rga.h>
#endif

namespace {
constexpr int kChannels = 3;
inline std::size_t bufferSizeBytes(int width, int height) {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kChannels;
}
}

bool RgaPreprocessor::initialize(int targetWidth, int targetHeight) {
    if (targetWidth <= 0 || targetHeight <= 0) {
        return false;
    }
    targetWidth_ = targetWidth;
    targetHeight_ = targetHeight;
    ensureBuffers();
    initialized_ = true;
    return true;
}

RgaPreprocessor::Result RgaPreprocessor::processBgr(const cv::Mat& frame) {
    Result result;
    if (!initialized_) {
        result.error = QStringLiteral("RGA 预处理尚未初始化");
        return result;
    }
    if (frame.empty()) {
        result.error = QStringLiteral("输入帧为空");
        return result;
    }
    if (frame.type() != CV_8UC3) {
        result.error = QStringLiteral("仅支持 CV_8UC3 帧");
        return result;
    }

    ensureBuffers();

    QElapsedTimer timer;
    timer.start();

#ifdef HAVE_RGA
    result = processWithRga(frame);
    if (result.success) {
        result.durationUs = timer.nsecsElapsed() / 1000;
        return result;
    }
#endif
    result = processWithCpu(frame);
    result.durationUs = timer.nsecsElapsed() / 1000;
    return result;
}

RgaPreprocessor::Result RgaPreprocessor::processNv12(const Nv12Input& input) {
    Result result;
    if (!initialized_) {
        result.error = QStringLiteral("RGA 预处理尚未初始化");
        return result;
    }
    if (input.width <= 0 || input.height <= 0) {
        result.error = QStringLiteral("NV12 输入尺寸非法");
        return result;
    }

    ensureBuffers();

    QElapsedTimer timer;
    timer.start();

#ifdef HAVE_RGA
    result = processNv12WithRga(input);
    if (result.success) {
        result.durationUs = timer.nsecsElapsed() / 1000;
        return result;
    }
#else
    Q_UNUSED(input);
#endif

    result = processNv12Cpu(input);
    result.durationUs = timer.nsecsElapsed() / 1000;
    return result;
}

void RgaPreprocessor::ensureBuffers() {
    const std::size_t requiredRgb = targetRgbBytes();
    if (outputBuffer_.size() < static_cast<int>(requiredRgb)) {
        outputBuffer_.resize(static_cast<int>(requiredRgb));
    }
    if (resizeScratchBuffer_.size() < static_cast<int>(requiredRgb)) {
        resizeScratchBuffer_.resize(static_cast<int>(requiredRgb));
    }
    const std::size_t requiredNv12 = targetNv12Bytes();
    if (nv12ScratchBuffer_.size() < static_cast<int>(requiredNv12)) {
        nv12ScratchBuffer_.resize(static_cast<int>(requiredNv12));
    }
}

RgaPreprocessor::Result RgaPreprocessor::processWithCpu(const cv::Mat& frame) {
    Result result;
    cv::Mat resized;
    if (frame.cols != targetWidth_ || frame.rows != targetHeight_) {
        cv::resize(frame, resized, cv::Size(targetWidth_, targetHeight_));
    } else {
        resized = frame;
    }

    cv::Mat target(targetHeight_, targetWidth_, CV_8UC3, outputBuffer_.data());
    cv::cvtColor(resized, target, cv::COLOR_BGR2RGB);

    result.success = true;
    result.data = reinterpret_cast<const uint8_t*>(outputBuffer_.constData());
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = targetWidth_ * kChannels;
    result.usedRga = false;
    return result;
}

RgaPreprocessor::Result RgaPreprocessor::processWithRga(const cv::Mat& frame) {
    Result result;
#ifdef HAVE_RGA
    rga_buffer_t src = wrapbuffer_virtualaddr(const_cast<uint8_t*>(frame.data), frame.cols, frame.rows, RK_FORMAT_BGR_888);
    rga_buffer_t resizedBuf = wrapbuffer_virtualaddr(resizeScratchBuffer_.data(), targetWidth_, targetHeight_, RK_FORMAT_BGR_888);

    IM_STATUS status = imresize(src, resizedBuf);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        result.error = QStringLiteral("RGA imresize 失败: %1").arg(QString::fromUtf8(imStrError(status)));
        result.success = false;
        return result;
    }

    rga_buffer_t rgbSrc = wrapbuffer_virtualaddr(resizeScratchBuffer_.data(), targetWidth_, targetHeight_, RK_FORMAT_BGR_888);
    rga_buffer_t rgbDst = wrapbuffer_virtualaddr(outputBuffer_.data(), targetWidth_, targetHeight_, RK_FORMAT_RGB_888);

    status = imcvtcolor(rgbSrc, rgbDst, RK_FORMAT_BGR_888, RK_FORMAT_RGB_888);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        result.error = QStringLiteral("RGA 转色失败: %1").arg(QString::fromUtf8(imStrError(status)));
        result.success = false;
        return result;
    }

    result.success = true;
    result.data = reinterpret_cast<const uint8_t*>(outputBuffer_.constData());
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = targetWidth_ * kChannels;
    result.usedRga = true;
#else
    Q_UNUSED(frame);
#endif
    return result;
}

RgaPreprocessor::Result RgaPreprocessor::processNv12WithRga(const Nv12Input& input) {
    Result result;
#ifdef HAVE_RGA
    const bool needResize = (input.width != targetWidth_) || (input.height != targetHeight_);
    const int srcStride = input.yStride > 0 ? input.yStride : input.width;
    rga_buffer_t srcBuffer;
    if (input.dmaFd >= 0) {
        srcBuffer = wrapbuffer_fd(input.dmaFd, input.width, input.height, RK_FORMAT_YCbCr_420_SP, srcStride, input.height);
    } else if (input.data) {
        srcBuffer = wrapbuffer_virtualaddr(const_cast<uint8_t*>(input.data + input.yPlaneOffset),
                                           input.width,
                                           input.height,
                                           RK_FORMAT_YCbCr_420_SP,
                                           srcStride,
                                           input.height);
    } else {
        result.error = QStringLiteral("NV12 数据指针缺失");
        return result;
    }

    rga_buffer_t workingSrc = srcBuffer;
    if (needResize) {
        rga_buffer_t resizeDst = wrapbuffer_virtualaddr(nv12ScratchBuffer_.data(),
                                                        targetWidth_,
                                                        targetHeight_,
                                                        RK_FORMAT_YCbCr_420_SP,
                                                        targetWidth_,
                                                        targetHeight_);
        IM_STATUS status = imresize(srcBuffer, resizeDst);
        if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
            result.error = QStringLiteral("RGA NV12 缩放失败: %1").arg(QString::fromUtf8(imStrError(status)));
            return result;
        }
        workingSrc = resizeDst;
    }

    rga_buffer_t dstRgb = wrapbuffer_virtualaddr(outputBuffer_.data(),
                                                 targetWidth_,
                                                 targetHeight_,
                                                 RK_FORMAT_RGB_888);
    IM_STATUS convertStatus = imcvtcolor(workingSrc, dstRgb, RK_FORMAT_YCbCr_420_SP, RK_FORMAT_RGB_888);
    if (convertStatus != IM_STATUS_SUCCESS && convertStatus != IM_STATUS_NOERROR) {
        result.error = QStringLiteral("RGA NV12 转 RGB 失败: %1").arg(QString::fromUtf8(imStrError(convertStatus)));
        return result;
    }

    result.success = true;
    result.data = reinterpret_cast<const uint8_t*>(outputBuffer_.constData());
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = targetWidth_ * kChannels;
    result.usedRga = true;
#else
    Q_UNUSED(input);
#endif
    return result;
}

RgaPreprocessor::Result RgaPreprocessor::processNv12Cpu(const Nv12Input& input) {
    Result result;
    if (!input.data) {
        result.error = QStringLiteral("NV12 数据指针缺失");
        return result;
    }

    const uint8_t* yPtr = input.data + input.yPlaneOffset;
    const uint8_t* uvPtr = input.data + input.uvPlaneOffset;
    const int yStride = input.yStride > 0 ? input.yStride : input.width;
    const int uvStride = input.uvStride > 0 ? input.uvStride : input.width;

    cv::Mat yPlane(input.height, input.width, CV_8UC1, const_cast<uint8_t*>(yPtr), yStride);
    cv::Mat uvPlane(input.height / 2, input.width / 2, CV_8UC2, const_cast<uint8_t*>(uvPtr), uvStride);
    cv::Mat rgbFull;
    cv::cvtColorTwoPlane(yPlane, uvPlane, rgbFull, cv::COLOR_YUV2RGB_NV12);

    cv::Mat target(targetHeight_, targetWidth_, CV_8UC3, outputBuffer_.data());
    if (input.width != targetWidth_ || input.height != targetHeight_) {
        cv::resize(rgbFull, target, cv::Size(targetWidth_, targetHeight_));
    } else {
        rgbFull.copyTo(target);
    }

    result.success = true;
    result.data = reinterpret_cast<const uint8_t*>(outputBuffer_.constData());
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = targetWidth_ * kChannels;
    result.usedRga = false;
    return result;
}

std::size_t RgaPreprocessor::targetRgbBytes() const {
    return bufferSizeBytes(targetWidth_, targetHeight_);
}

std::size_t RgaPreprocessor::targetNv12Bytes() const {
    return static_cast<std::size_t>(targetWidth_) * static_cast<std::size_t>(targetHeight_) * 3 / 2;
}
