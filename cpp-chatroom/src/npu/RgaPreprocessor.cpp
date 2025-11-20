#include "npu/RgaPreprocessor.h"

#include <QtCore/QElapsedTimer>
#include <QtCore/QLoggingCategory>
#include <QDebug>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

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

RgaPreprocessor::Result RgaPreprocessor::processNv12ToDisplay(const Nv12Input& input, int displayWidth, int displayHeight) {
    Result result;
    if (displayWidth <= 0 || displayHeight <= 0) {
        result.error = QStringLiteral("显示尺寸非法");
        return result;
    }

    // 确保显示缓冲大小足够
    const int stride = displayWidth * 3; // RGB888
    const int requiredSize = stride * displayHeight;
    if (displayBuffer_.size() < requiredSize) {
        displayBuffer_.resize(requiredSize);
    }

    // 准备 RGA 输入
    rga_buffer_t srcBuffer;
    const int srcStride = input.yStride > 0 ? input.yStride : input.width;
    
#ifdef HAVE_RGA
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
        result.error = QStringLiteral("NV12 数据缺失");
        return result;
    }

    // 准备 RGA 输出 (RGB888)
    uint8_t* dstPtr = reinterpret_cast<uint8_t*>(displayBuffer_.data());
    rga_buffer_t dstBuffer = wrapbuffer_virtualaddr(dstPtr, displayWidth, displayHeight, RK_FORMAT_RGB_888);

    // 执行 RGA 缩放 + 转换
    // 使用 imresize 直接缩放全图到显示尺寸
    IM_STATUS status = imresize(srcBuffer, dstBuffer);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        result.error = QStringLiteral("RGA 显示转换失败: %1").arg(QString::fromUtf8(imStrError(status)));
        return result;
    }

    result.success = true;
    result.data = dstPtr;
    result.width = displayWidth;
    result.height = displayHeight;
    result.stride = stride;
    result.usedRga = true;
#else
    // CPU 回退路径 (仅作兼容，性能较低)
    Q_UNUSED(srcBuffer);
    result.error = QStringLiteral("未启用 RGA，无法加速显示转换");
#endif
    return result;
}

void RgaPreprocessor::ensureBuffers() {
    const std::size_t requiredRgb = targetRgbBytes();
    if (outputBuffer_.size() < static_cast<int>(requiredRgb)) {
        outputBuffer_.resize(static_cast<int>(requiredRgb));
    }
}

RgaPreprocessor::Result RgaPreprocessor::processWithCpu(const cv::Mat& frame) {
    Result result;
    const LetterboxTransform letterbox = computeLetterbox(frame.cols, frame.rows);
    result.letterbox = letterbox;

    cv::Mat rgbSource;
    cv::cvtColor(frame, rgbSource, cv::COLOR_BGR2RGB);

    cv::Mat resized;
    if (letterbox.contentWidth != rgbSource.cols || letterbox.contentHeight != rgbSource.rows) {
        cv::resize(rgbSource, resized, cv::Size(letterbox.contentWidth, letterbox.contentHeight));
    } else {
        resized = rgbSource;
    }

    int strideBytes = 0;
    uint8_t* targetPtr = resolveOutputBuffer(nullptr, 0, strideBytes);
    clearTargetBuffer(targetPtr, strideBytes);

    if (letterbox.contentWidth > 0 && letterbox.contentHeight > 0) {
        cv::Mat target(targetHeight_, targetWidth_, CV_8UC3, targetPtr, strideBytes);
        cv::Rect roi(letterbox.padLeft, letterbox.padTop, letterbox.contentWidth, letterbox.contentHeight);
        resized.copyTo(target(roi));
    }

    result.success = true;
    result.data = targetPtr;
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = strideBytes;
    result.usedRga = false;
    return result;
}

RgaPreprocessor::Result RgaPreprocessor::processWithRga(const cv::Mat& frame) {
    Result result;
#ifdef HAVE_RGA
    const LetterboxTransform letterbox = computeLetterbox(frame.cols, frame.rows);
    result.letterbox = letterbox;
    if (letterbox.contentWidth <= 0 || letterbox.contentHeight <= 0) {
        result.error = QStringLiteral("Letterbox 计算失败");
        return result;
    }

    int strideBytes = 0;
    uint8_t* targetPtr = resolveOutputBuffer(nullptr, 0, strideBytes);
    clearTargetBuffer(targetPtr, strideBytes);

    rga_buffer_t src = wrapbuffer_virtualaddr(const_cast<uint8_t*>(frame.data), frame.cols, frame.rows, RK_FORMAT_BGR_888);
    
    // Use RGA to directly resize and convert into the ROI of the target buffer
    uint8_t* dstBase = targetPtr + letterbox.padTop * strideBytes + letterbox.padLeft * kChannels;
    rga_buffer_t dst = wrapbuffer_virtualaddr(dstBase,
                                              letterbox.contentWidth,
                                              letterbox.contentHeight,
                                              RK_FORMAT_RGB_888,
                                              targetWidth_,
                                              targetHeight_);

    IM_STATUS status = imresize(src, dst);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        const QString err = QStringLiteral("RGA imresize (BGR->RGB) 失败: %1").arg(QString::fromUtf8(imStrError(status)));
        qWarning() << err;
        result.error = err;
        return result;
    }

    result.success = true;
    result.data = targetPtr;
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = strideBytes;
    result.usedRga = true;
#else
    Q_UNUSED(frame);
#endif
    return result;
}

RgaPreprocessor::Result RgaPreprocessor::processNv12WithRga(const Nv12Input& input) {
    Result result;
#ifdef HAVE_RGA
    const LetterboxTransform letterbox = computeLetterbox(input.width, input.height);
    result.letterbox = letterbox;
    if (letterbox.contentWidth <= 0 || letterbox.contentHeight <= 0) {
        result.error = QStringLiteral("Letterbox 计算失败");
        return result;
    }

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

    int strideBytes = 0;
    uint8_t* targetPtr = resolveOutputBuffer(input.targetVirtual, input.targetStride, strideBytes);
    clearTargetBuffer(targetPtr, strideBytes);

    // Use RGA to directly resize and convert into the ROI of the target buffer
    // We prefer using FD if available for zero-copy
    if (input.targetDmaFd >= 0) {
        rga_buffer_t dst = wrapbuffer_fd(input.targetDmaFd, targetWidth_, targetHeight_, RK_FORMAT_RGB_888);
        
        im_rect srcRect = {0, 0, input.width, input.height};
        im_rect dstRect = {letterbox.padLeft, letterbox.padTop, letterbox.contentWidth, letterbox.contentHeight};
        
        // usage: improcess(src, dst, src_rect, dst_rect, resize_rect, usage)
        // Note: imresize is a wrapper around improcess. 
        // To use specific rects, we should use improcess directly.
        // improcess(src, dst, pat, srect, drect, prect, acquire_fence_fd)
        
        IM_STATUS status = improcess(srcBuffer, dst, {}, srcRect, dstRect, {}, 0);
        if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
             const QString err = QStringLiteral("RGA improcess (NV12->RGB FD) 失败: %1").arg(QString::fromUtf8(imStrError(status)));
             qWarning() << err;
             result.error = err;
             return result;
        }
    } else {
        uint8_t* dstBase = targetPtr + letterbox.padTop * strideBytes + letterbox.padLeft * kChannels;
        rga_buffer_t dst = wrapbuffer_virtualaddr(dstBase,
                                                letterbox.contentWidth,
                                                letterbox.contentHeight,
                                                RK_FORMAT_RGB_888,
                                                targetWidth_,
                                                targetHeight_);

        IM_STATUS status = imresize(srcBuffer, dst);
        if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
            const QString err = QStringLiteral("RGA imresize (NV12->RGB Virt) 失败: %1").arg(QString::fromUtf8(imStrError(status)));
            qWarning() << err;
            result.error = err;
            return result;
        }
    }

    result.success = true;
    result.data = targetPtr;
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = strideBytes;
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

    const LetterboxTransform letterbox = computeLetterbox(input.width, input.height);
    result.letterbox = letterbox;

    cv::Mat resized;
    if (letterbox.contentWidth != rgbFull.cols || letterbox.contentHeight != rgbFull.rows) {
        cv::resize(rgbFull, resized, cv::Size(letterbox.contentWidth, letterbox.contentHeight));
    } else {
        resized = rgbFull;
    }

    int strideBytes = 0;
    uint8_t* targetPtr = resolveOutputBuffer(input.targetVirtual, input.targetStride, strideBytes);
    clearTargetBuffer(targetPtr, strideBytes);

    if (letterbox.contentWidth > 0 && letterbox.contentHeight > 0) {
        cv::Mat target(targetHeight_, targetWidth_, CV_8UC3, targetPtr, strideBytes);
        cv::Rect roi(letterbox.padLeft, letterbox.padTop, letterbox.contentWidth, letterbox.contentHeight);
        resized.copyTo(target(roi));
    }

    result.success = true;
    result.data = targetPtr;
    result.width = targetWidth_;
    result.height = targetHeight_;
    result.stride = strideBytes;
    result.usedRga = false;
    return result;
}

std::size_t RgaPreprocessor::targetRgbBytes() const {
    return bufferSizeBytes(targetWidth_, targetHeight_);
}

std::size_t RgaPreprocessor::targetNv12Bytes() const {
    return static_cast<std::size_t>(targetWidth_) * static_cast<std::size_t>(targetHeight_) * 3 / 2;
}

LetterboxTransform RgaPreprocessor::computeLetterbox(int srcWidth, int srcHeight) const {
    LetterboxTransform info;
    if (srcWidth <= 0 || srcHeight <= 0 || targetWidth_ <= 0 || targetHeight_ <= 0) {
        return info;
    }
    const float scaleW = static_cast<float>(targetWidth_) / static_cast<float>(srcWidth);
    const float scaleH = static_cast<float>(targetHeight_) / static_cast<float>(srcHeight);
    const float scale = std::min(scaleW, scaleH);
    const int contentWidth = std::max(1, static_cast<int>(std::round(static_cast<float>(srcWidth) * scale)));
    const int contentHeight = std::max(1, static_cast<int>(std::round(static_cast<float>(srcHeight) * scale)));

    info.scaleX = scale;
    info.scaleY = scale;
    info.contentWidth = std::min(contentWidth, targetWidth_);
    info.contentHeight = std::min(contentHeight, targetHeight_);
    
    // Align padLeft to ensure RGA destination address is aligned.
    // RGA typically requires 16-byte alignment for base addresses.
    // Since we are using RGB888 (3 bytes/pixel), we need padLeft * 3 to be a multiple of 16?
    // Actually, let's try to align padLeft to 16 pixels to be safe and simple.
    // 16 pixels * 3 bytes = 48 bytes, which is 16-byte aligned.
    int rawPadLeft = (targetWidth_ - info.contentWidth) / 2;
    info.padLeft = (rawPadLeft / 16) * 16;
    
    info.padTop = (targetHeight_ - info.contentHeight) / 2;
    return info;
}

uint8_t* RgaPreprocessor::resolveOutputBuffer(uint8_t* preferred, int preferredStride, int& actualStrideBytes) {
    const int defaultStride = targetWidth_ * kChannels;
    if (preferred) {
        actualStrideBytes = (preferredStride > 0) ? preferredStride : defaultStride;
        return preferred;
    }
    actualStrideBytes = defaultStride;
    return reinterpret_cast<uint8_t*>(outputBuffer_.data());
}

void RgaPreprocessor::clearTargetBuffer(uint8_t* target, int strideBytes) {
    if (!target || strideBytes <= 0) {
        return;
    }
    std::memset(target, 0, strideBytes * targetHeight_);
}

void RgaPreprocessor::blitIntoLetterbox(const LetterboxTransform& letterbox,
                                        uint8_t* target,
                                        int targetStrideBytes,
                                        const uint8_t* src,
                                        int srcStrideBytes) {
    if (!target || !src || targetStrideBytes <= 0 || srcStrideBytes <= 0) {
        return;
    }
    if (letterbox.contentWidth <= 0 || letterbox.contentHeight <= 0) {
        return;
    }

    const int copyBytesPerRow = letterbox.contentWidth * kChannels;
    uint8_t* dstRow = target + letterbox.padTop * targetStrideBytes + letterbox.padLeft * kChannels;
    const uint8_t* srcRow = src;
    for (int row = 0; row < letterbox.contentHeight; ++row) {
        std::memcpy(dstRow, srcRow, static_cast<std::size_t>(copyBytesPerRow));
        dstRow += targetStrideBytes;
        srcRow += srcStrideBytes;
    }
}
