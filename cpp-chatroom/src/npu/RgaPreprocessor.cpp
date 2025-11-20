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

void RgaPreprocessor::ensureBuffers() {
    const std::size_t required = bufferSizeBytes(targetWidth_, targetHeight_);
    if (outputBuffer_.size() < static_cast<int>(required)) {
        outputBuffer_.resize(static_cast<int>(required));
    }
    if (resizeScratchBuffer_.size() < static_cast<int>(required)) {
        resizeScratchBuffer_.resize(static_cast<int>(required));
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
