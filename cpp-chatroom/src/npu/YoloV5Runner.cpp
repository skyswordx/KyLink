// Adapted from Rockchip RKNN YOLOv5 sample code (Apache License 2.0).

#include "npu/YoloV5Runner.h"

#include "npu/postprocess.h"

#include "backend/PerformanceMonitor.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <rknn_api.h>

namespace {

void dumpTensorAttr(rknn_tensor_attr *attr) {
    if (!attr) {
        return;
    }
    printf("\tindex=%d, name=%s, \n\t\tn_dims=%d, dims=[%d, %d, %d, %d], \n\t\tn_elems=%d, size=%d, fmt=%d, \n\t\ttype=%d, qnt_type=%d, zp=%d, scale=%f\n",
           attr->index,
           attr->name ? attr->name : "",
           attr->n_dims,
           attr->dims[0],
           attr->dims[1],
           attr->dims[2],
           attr->dims[3],
           attr->n_elems,
           attr->size,
           static_cast<int>(attr->fmt),
           static_cast<int>(attr->type),
           static_cast<int>(attr->qnt_type),
           attr->zp,
           attr->scale);
}

bool loadModelFile(const QString &path, std::vector<unsigned char> &buffer, QString &errorOut) {
    QFile file(path);
    if (!file.exists()) {
        errorOut = QStringLiteral("模型文件不存在: %1").arg(path);
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        errorOut = QStringLiteral("无法打开模型文件: %1").arg(path);
        return false;
    }

    const qint64 size = file.size();
    if (size <= 0) {
        errorOut = QStringLiteral("模型文件大小非法: %1").arg(path);
        return false;
    }

    buffer.resize(static_cast<size_t>(size));
    qint64 bytesRead = file.read(reinterpret_cast<char *>(buffer.data()), size);
    if (bytesRead != size) {
        errorOut = QStringLiteral("读取模型文件失败: %1").arg(path);
        buffer.clear();
        return false;
    }

    return true;
}

}  // namespace

YoloV5Runner::YoloV5Runner()
        : ctx_(0),
            ready_(false),
            inputWidth_(0),
            inputHeight_(0),
            inputChannel_(0),
            ioNum_{},
            inputMem_(nullptr),
            inputTensorView_{} {}

YoloV5Runner::~YoloV5Runner() {
    release();
}

void YoloV5Runner::release() {
    releaseInputMemory();
    if (ctx_ != 0) {
        PerformanceMonitor::instance()->setNpuContext(0);
        rknn_destroy(ctx_);
        ctx_ = 0;
    }
    ready_ = false;
    inputWidth_ = 0;
    inputHeight_ = 0;
    inputChannel_ = 0;
    ioNum_ = rknn_input_output_num{};
    inputAttrs_.clear();
    outputAttrs_.clear();
    outputScales_.clear();
    outputZps_.clear();
}

bool YoloV5Runner::loadModel(const QString &modelPath, QString *errorOut) {
    QString dummyError;
    QString &err = errorOut ? *errorOut : dummyError;
    err.clear();

    QFileInfo modelInfo(modelPath);
    if (!modelInfo.exists()) {
        err = QStringLiteral("模型文件不存在: %1").arg(modelPath);
        return false;
    }

    std::vector<unsigned char> modelData;
    if (!loadModelFile(modelPath, modelData, err)) {
        return false;
    }

    release();

    int ret = rknn_init(&ctx_, modelData.data(), modelData.size(), 0, nullptr);
    if (ret < 0) {
        err = QStringLiteral("rknn_init 失败, 错误码: %1").arg(ret);
        release();
        return false;
    }

    rknn_sdk_version version{};
    ret = rknn_query(ctx_, RKNN_QUERY_SDK_VERSION, &version, sizeof(version));
    if (ret < 0) {
        err = QStringLiteral("查询 SDK 版本失败, 错误码: %1").arg(ret);
        release();
        return false;
    }
    printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);

    ret = rknn_query(ctx_, RKNN_QUERY_IN_OUT_NUM, &ioNum_, sizeof(ioNum_));
    if (ret < 0) {
        err = QStringLiteral("查询 IO 数失败, 错误码: %1").arg(ret);
        release();
        return false;
    }

    inputAttrs_.resize(ioNum_.n_input);
    for (uint32_t i = 0; i < ioNum_.n_input; ++i) {
        inputAttrs_[i].index = i;
        ret = rknn_query(ctx_, RKNN_QUERY_INPUT_ATTR, &inputAttrs_[i], sizeof(rknn_tensor_attr));
        if (ret < 0) {
            err = QStringLiteral("查询输入属性失败, 错误码: %1").arg(ret);
            release();
            return false;
        }
        dumpTensorAttr(&inputAttrs_[i]);
    }

    if (inputAttrs_.empty()) {
        err = QStringLiteral("模型未暴露任何输入");
        release();
        return false;
    }

    if (!inputAttrs_.empty() && inputAttrs_[0].fmt == RKNN_TENSOR_NCHW) {
        inputChannel_ = inputAttrs_[0].dims[1];
        inputHeight_ = inputAttrs_[0].dims[2];
        inputWidth_ = inputAttrs_[0].dims[3];
    } else if (!inputAttrs_.empty()) {
        inputHeight_ = inputAttrs_[0].dims[1];
        inputWidth_ = inputAttrs_[0].dims[2];
        inputChannel_ = inputAttrs_[0].dims[3];
    }

    if (inputChannel_ != 3) {
        err = QStringLiteral("仅支持三通道模型, 当前通道数: %1").arg(inputChannel_);
        release();
        return false;
    }

    printf("model input height=%d, width=%d, channel=%d\n", inputHeight_, inputWidth_, inputChannel_);

    if (ioNum_.n_output < 3) {
        err = QStringLiteral("模型输出节点不足: %1").arg(ioNum_.n_output);
        release();
        return false;
    }

    outputAttrs_.resize(ioNum_.n_output);
    outputScales_.clear();
    outputZps_.clear();
    outputScales_.reserve(ioNum_.n_output);
    outputZps_.reserve(ioNum_.n_output);

    for (uint32_t i = 0; i < ioNum_.n_output; ++i) {
        outputAttrs_[i].index = i;
        ret = rknn_query(ctx_, RKNN_QUERY_OUTPUT_ATTR, &outputAttrs_[i], sizeof(rknn_tensor_attr));
        if (ret < 0) {
            err = QStringLiteral("查询输出属性失败, 错误码: %1").arg(ret);
            release();
            return false;
        }
        dumpTensorAttr(&outputAttrs_[i]);
        outputScales_.push_back(outputAttrs_[i].scale);
        outputZps_.push_back(outputAttrs_[i].zp);
    }

    if (!initializeInputMemory(err)) {
        ready_ = false;
        release();
        return false;
    }

    ready_ = true;
    PerformanceMonitor::instance()->setNpuContext(ctx_);
    return true;
}

bool YoloV5Runner::isReady() const {
    return ready_;
}

int YoloV5Runner::inputWidth() const {
    return inputWidth_;
}

int YoloV5Runner::inputHeight() const {
    return inputHeight_;
}

YoloV5Runner::InputTensorView YoloV5Runner::inputTensorView() const {
    return inputTensorView_;
}

bool YoloV5Runner::infer(const cv::Mat &frameBgr,
                         DetectResultGroup *resultOut,
                         std::int64_t *inferenceTimeMs,
                         cv::Mat *visualizedFrame,
                         QString *errorOut,
                         InferenceBreakdown *breakdownOut) {
    QString dummyError;
    QString &err = errorOut ? *errorOut : dummyError;
    err.clear();

    if (!ready_) {
        err = QStringLiteral("模型尚未加载");
        return false;
    }
    if (frameBgr.empty()) {
        err = QStringLiteral("输入帧为空");
        return false;
    }

    if (resultOut) {
        *resultOut = DetectResultGroup{};
    }
    DetectResultGroup localGroup{};
    DetectResultGroup *group = resultOut ? resultOut : &localGroup;

    InferenceBreakdown dummyBreakdown;
    InferenceBreakdown *breakdown = breakdownOut ? breakdownOut : &dummyBreakdown;
    *breakdown = InferenceBreakdown{};

    const auto preprocessStart = std::chrono::steady_clock::now();

    cv::Mat rgb_img;
    cv::cvtColor(frameBgr, rgb_img, cv::COLOR_BGR2RGB);

    cv::Mat resized_img;
    const cv::Mat *inputMat = &rgb_img;
    if (frameBgr.cols != inputWidth_ || frameBgr.rows != inputHeight_) {
        cv::resize(rgb_img, resized_img, cv::Size(inputWidth_, inputHeight_));
        inputMat = &resized_img;
    }

    const auto preprocessEnd = std::chrono::steady_clock::now();
    breakdown->preprocessUs = std::chrono::duration_cast<std::chrono::microseconds>(preprocessEnd - preprocessStart).count();

    LetterboxTransform letterbox;
    letterbox.scaleX = inputWidth_ > 0 && frameBgr.cols > 0 ? static_cast<float>(inputWidth_) / static_cast<float>(frameBgr.cols) : 1.f;
    letterbox.scaleY = inputHeight_ > 0 && frameBgr.rows > 0 ? static_cast<float>(inputHeight_) / static_cast<float>(frameBgr.rows) : 1.f;
    letterbox.contentWidth = inputWidth_;
    letterbox.contentHeight = inputHeight_;

    const uint8_t *bufferPtr = inputMat->data;
    bool matchesBound = false;
    if (inputTensorView_.isValid() && inputBufferBytes() > 0) {
        std::memcpy(inputTensorView_.data, bufferPtr, inputBufferBytes());
        bufferPtr = inputTensorView_.data;
        matchesBound = true;
    }

    return runInferenceInternal(frameBgr,
                                frameBgr.cols,
                                frameBgr.rows,
                                bufferPtr,
                                matchesBound,
                                letterbox,
                                err,
                                group,
                                inferenceTimeMs,
                                visualizedFrame,
                                breakdown);
}

bool YoloV5Runner::inferFromRgbBuffer(const cv::Mat &originalFrameBgr,
                                      int originalWidth,
                                      int originalHeight,
                                      const uint8_t *rgbData,
                                      int rgbStride,
                                      const LetterboxTransform &letterbox,
                                      DetectResultGroup *resultOut,
                                      std::int64_t *inferenceTimeMs,
                                      cv::Mat *visualizedFrame,
                                      QString *errorOut,
                                      InferenceBreakdown *breakdownOut) {
    QString dummyError;
    QString &err = errorOut ? *errorOut : dummyError;
    err.clear();

    if (!ready_) {
        err = QStringLiteral("模型尚未加载");
        return false;
    }
    if (!rgbData) {
        err = QStringLiteral("输入数据为空");
        return false;
    }
    if (rgbStride != inputWidth_ * inputChannel_) {
        err = QStringLiteral("RGB stride 非预期: %1").arg(rgbStride);
        return false;
    }

    DetectResultGroup localGroup;
    DetectResultGroup *group = resultOut ? resultOut : &localGroup;
    *group = DetectResultGroup{};

    InferenceBreakdown dummyBreakdown;
    InferenceBreakdown *breakdown = breakdownOut ? breakdownOut : &dummyBreakdown;

    const int width = originalWidth > 0 ? originalWidth : originalFrameBgr.cols;
    const int height = originalHeight > 0 ? originalHeight : originalFrameBgr.rows;
    if (width <= 0 || height <= 0) {
        err = QStringLiteral("原始分辨率非法");
        return false;
    }

    bool matchesBoundMemory = inputTensorView_.isValid() && rgbData == inputTensorView_.data;
    const std::size_t expectedBytes = inputBufferBytes();
    if (!matchesBoundMemory && inputTensorView_.isValid() && expectedBytes > 0) {
        std::memcpy(inputTensorView_.data, rgbData, expectedBytes);
        rgbData = inputTensorView_.data;
        matchesBoundMemory = true;
    }

    return runInferenceInternal(originalFrameBgr,
                                width,
                                height,
                                rgbData,
                                matchesBoundMemory,
                                letterbox,
                                err,
                                group,
                                inferenceTimeMs,
                                visualizedFrame,
                                breakdown);
}

bool YoloV5Runner::runInferenceInternal(const cv::Mat &originalFrameBgr,
                                        int originalWidth,
                                        int originalHeight,
                                        const uint8_t *inputBuffer,
                                        bool inputMatchesBoundMemory,
                                        const LetterboxTransform &letterbox,
                                        QString &err,
                                        DetectResultGroup *group,
                                        std::int64_t *inferenceTimeMs,
                                        cv::Mat *visualizedFrame,
                                        InferenceBreakdown *breakdown) {
    if (!inputBuffer) {
        err = QStringLiteral("输入缓冲区为空");
        return false;
    }

    if (!inputMatchesBoundMemory || !inputTensorView_.isValid()) {
        rknn_input inputs[1];
        std::memset(inputs, 0, sizeof(inputs));
        inputs[0].index = 0;
        inputs[0].type = RKNN_TENSOR_UINT8;
        inputs[0].size = inputWidth_ * inputHeight_ * inputChannel_;
        inputs[0].fmt = RKNN_TENSOR_NHWC;
        inputs[0].pass_through = 0;
        inputs[0].buf = const_cast<uint8_t *>(inputBuffer);

        int setRet = rknn_inputs_set(ctx_, ioNum_.n_input, inputs);
        if (setRet < 0) {
            err = QStringLiteral("设置输入失败, 错误码: %1").arg(setRet);
            return false;
        }
    }

    std::vector<rknn_output> outputs(ioNum_.n_output);
    for (uint32_t i = 0; i < ioNum_.n_output; ++i) {
        outputs[i].index = i;
        outputs[i].want_float = 0;
    }

    const auto npuStart = std::chrono::steady_clock::now();
    int ret = rknn_run(ctx_, nullptr);
    if (ret < 0) {
        err = QStringLiteral("推理失败, 错误码: %1").arg(ret);
        return false;
    }

    ret = rknn_outputs_get(ctx_, ioNum_.n_output, outputs.data(), nullptr);
    if (ret < 0) {
        err = QStringLiteral("获取输出失败, 错误码: %1").arg(ret);
        return false;
    }

    const auto npuEnd = std::chrono::steady_clock::now();
    if (breakdown) {
        breakdown->npuUs = std::chrono::duration_cast<std::chrono::microseconds>(npuEnd - npuStart).count();
    }

    const int processedWidth = originalWidth > 0 ? originalWidth : inputWidth_;
    const int processedHeight = originalHeight > 0 ? originalHeight : inputHeight_;

    const auto postStart = std::chrono::steady_clock::now();
    ret = post_process(static_cast<int8_t *>(outputs[0].buf),
                       static_cast<int8_t *>(outputs[1].buf),
                       static_cast<int8_t *>(outputs[2].buf),
                       inputHeight_,
                       inputWidth_,
                       processedHeight,
                       processedWidth,
                       BOX_THRESH,
                       NMS_THRESH,
                       letterbox,
                       outputZps_,
                       outputScales_,
                       group);
    if (ret < 0) {
        err = QStringLiteral("后处理失败");
        rknn_outputs_release(ctx_, ioNum_.n_output, outputs.data());
        return false;
    }

    const auto postEnd = std::chrono::steady_clock::now();
    if (breakdown) {
        breakdown->postprocessUs = std::chrono::duration_cast<std::chrono::microseconds>(postEnd - postStart).count();
    }

    if (inferenceTimeMs && breakdown) {
        const qint64 combinedUs = std::max<qint64>(0, breakdown->npuUs) + std::max<qint64>(0, breakdown->postprocessUs);
        *inferenceTimeMs = combinedUs / 1000;
    }

    if (visualizedFrame) {
        if (!originalFrameBgr.empty()) {
            originalFrameBgr.copyTo(*visualizedFrame);
        } else {
            *visualizedFrame = cv::Mat(originalHeight, originalWidth, CV_8UC3);
        }
        for (int i = 0; i < group->count; ++i) {
            const DetectResult &det = group->results[i];
            cv::rectangle(*visualizedFrame,
                          cv::Point(det.box.left, det.box.top),
                          cv::Point(det.box.right, det.box.bottom),
                          cv::Scalar(255, 0, 0),
                          2);

            char text[256];
            std::snprintf(text, sizeof(text), "%s %.1f%%", det.name, det.prop * 100.f);
            int baseLine = 0;
            cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
            int x = det.box.left;
            int y = det.box.top - label_size.height - baseLine;
            if (y < 0) y = 0;
            if (x + label_size.width > visualizedFrame->cols) x = visualizedFrame->cols - label_size.width;

            cv::rectangle(*visualizedFrame,
                          cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                          cv::Scalar(255, 255, 255),
                          cv::FILLED);
            cv::putText(*visualizedFrame,
                        text,
                        cv::Point(x, y + label_size.height),
                        cv::FONT_HERSHEY_SIMPLEX,
                        0.5,
                        cv::Scalar(0, 0, 0),
                        1);
        }
    }

    rknn_outputs_release(ctx_, ioNum_.n_output, outputs.data());
    return true;
}

bool YoloV5Runner::initializeInputMemory(QString &err) {
    Q_UNUSED(err);
    // RK3566 性能优化：
    // 禁用 rknn_create_mem 零拷贝分配，强制使用 rknn_inputs_set 的拷贝模式。
    // 零拷贝模式虽然减少了 CPU 拷贝，但会导致 NPU 推理时的内存访问延迟大幅增加（+30ms）。
    // 当前方案：RGA 预处理 -> System Memory -> CPU memcpy -> NPU Internal Memory
    releaseInputMemory();
    inputTensorView_ = InputTensorView{};
    return true;
}

void YoloV5Runner::releaseInputMemory() {
    if (inputMem_ && ctx_ != 0) {
        rknn_destroy_mem(ctx_, inputMem_);
    }
    inputMem_ = nullptr;
    inputTensorView_ = InputTensorView{};
}

std::size_t YoloV5Runner::inputBufferBytes() const {
    if (inputWidth_ <= 0 || inputHeight_ <= 0 || inputChannel_ <= 0) {
        return 0;
    }
    return static_cast<std::size_t>(inputWidth_) * static_cast<std::size_t>(inputHeight_) * static_cast<std::size_t>(inputChannel_);
}

bool YoloV5Runner::runSample(const QString &modelPath,
                             const QString &inputImagePath,
                             const QString &outputImagePath,
                             QString *errorOut) {
    QString dummyError;
    QString &err = errorOut ? *errorOut : dummyError;
    err.clear();

    QFileInfo imageInfo(inputImagePath);
    if (!imageInfo.exists()) {
        err = QStringLiteral("输入图像不存在: %1").arg(inputImagePath);
        return false;
    }

    YoloV5Runner runner;
    if (!runner.loadModel(modelPath, &err)) {
        return false;
    }

    cv::Mat orig_img = cv::imread(inputImagePath.toStdString(), cv::IMREAD_COLOR);
    if (orig_img.empty()) {
        err = QStringLiteral("无法读取输入图片: %1").arg(inputImagePath);
        return false;
    }

    cv::Mat vis_img;
    DetectResultGroup detections{};
    std::int64_t duration_ms = 0;
    if (!runner.infer(orig_img, &detections, &duration_ms, &vis_img, &err, nullptr)) {
        return false;
    }

    if (!cv::imwrite(outputImagePath.toStdString(), vis_img)) {
        err = QStringLiteral("写出检测结果失败: %1").arg(outputImagePath);
        return false;
    }

    qInfo().noquote() << QStringLiteral("NPU 检测输出: %1 (共 %2 个目标, 推理 %3 ms)")
                             .arg(outputImagePath)
                             .arg(detections.count)
                             .arg(duration_ms);
    return true;
}
