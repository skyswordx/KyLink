// Adapted from Rockchip RKNN YOLOv5 sample code (Apache License 2.0).

#include "npu/YoloV5Runner.h"

#include "npu/postprocess.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>

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

bool YoloV5Runner::runSample(const QString &modelPath,
                             const QString &inputImagePath,
                             const QString &outputImagePath,
                             QString *errorOut) const {
    QString dummyError;
    QString &err = errorOut ? *errorOut : dummyError;
    err.clear();

    QFileInfo modelInfo(modelPath);
    QFileInfo imageInfo(inputImagePath);
    if (!modelInfo.exists()) {
        err = QStringLiteral("模型文件不存在: %1").arg(modelPath);
        return false;
    }
    if (!imageInfo.exists()) {
        err = QStringLiteral("输入图像不存在: %1").arg(inputImagePath);
        return false;
    }

    std::vector<unsigned char> modelData;
    if (!loadModelFile(modelPath, modelData, err)) {
        return false;
    }

    rknn_context ctx = 0;
    int ret = rknn_init(&ctx, modelData.data(), modelData.size(), 0, nullptr);
    if (ret < 0) {
        err = QStringLiteral("rknn_init 失败, 错误码: %1").arg(ret);
        return false;
    }

    rknn_sdk_version version{};
    ret = rknn_query(ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(version));
    if (ret < 0) {
        err = QStringLiteral("查询 SDK 版本失败, 错误码: %1").arg(ret);
        rknn_destroy(ctx);
        return false;
    }
    printf("sdk version: %s driver version: %s\n", version.api_version, version.drv_version);

    rknn_input_output_num io_num{};
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        err = QStringLiteral("查询 IO 数失败, 错误码: %1").arg(ret);
        rknn_destroy(ctx);
        return false;
    }

    printf("\nmodel input num: %d\n", io_num.n_input);
    std::vector<rknn_tensor_attr> input_attrs(io_num.n_input);
    for (uint32_t i = 0; i < io_num.n_input; ++i) {
        input_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &input_attrs[i], sizeof(rknn_tensor_attr));
        if (ret < 0) {
            err = QStringLiteral("查询输入属性失败, 错误码: %1").arg(ret);
            rknn_destroy(ctx);
            return false;
        }
        dumpTensorAttr(&input_attrs[i]);
    }

    printf("\nmodel output num: %d\n", io_num.n_output);
    std::vector<rknn_tensor_attr> output_attrs(io_num.n_output);
    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        output_attrs[i].index = i;
        ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &output_attrs[i], sizeof(rknn_tensor_attr));
        if (ret < 0) {
            err = QStringLiteral("查询输出属性失败, 错误码: %1").arg(ret);
            rknn_destroy(ctx);
            return false;
        }
        dumpTensorAttr(&output_attrs[i]);
    }

    int channel = 3;
    int width = 0;
    int height = 0;
    if (input_attrs[0].fmt == RKNN_TENSOR_NCHW) {
        printf("\nmodel input is NCHW\n");
        channel = input_attrs[0].dims[1];
        height = input_attrs[0].dims[2];
        width = input_attrs[0].dims[3];
    } else {
        printf("\nmodel input is NHWC\n");
        height = input_attrs[0].dims[1];
        width = input_attrs[0].dims[2];
        channel = input_attrs[0].dims[3];
    }

    if (channel != 3) {
        err = QStringLiteral("仅支持三通道模型, 当前通道数: %1").arg(channel);
        rknn_destroy(ctx);
        return false;
    }

    printf("model input height=%d, width=%d, channel=%d\n\n", height, width, channel);

    cv::Mat orig_img = cv::imread(inputImagePath.toStdString(), cv::IMREAD_COLOR);
    if (orig_img.empty()) {
        err = QStringLiteral("无法读取输入图片: %1").arg(inputImagePath);
        rknn_destroy(ctx);
        return false;
    }

    cv::Mat rgb_img;
    cv::cvtColor(orig_img, rgb_img, cv::COLOR_BGR2RGB);

    cv::Mat resized_img;
    const int img_width = rgb_img.cols;
    const int img_height = rgb_img.rows;

    rknn_input inputs[1];
    std::memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].size = width * height * channel;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].pass_through = 0;

    if (img_width != width || img_height != height) {
        cv::resize(rgb_img, resized_img, cv::Size(width, height));
        inputs[0].buf = resized_img.data;
    } else {
        inputs[0].buf = rgb_img.data;
    }

    ret = rknn_inputs_set(ctx, io_num.n_input, inputs);
    if (ret < 0) {
        err = QStringLiteral("设置输入失败, 错误码: %1").arg(ret);
        rknn_destroy(ctx);
        return false;
    }

    std::vector<rknn_output> outputs(io_num.n_output);
    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        outputs[i].index = i;
        outputs[i].want_float = 0;
    }

    auto start = std::chrono::steady_clock::now();
    ret = rknn_run(ctx, nullptr);
    if (ret < 0) {
        err = QStringLiteral("推理失败, 错误码: %1").arg(ret);
        rknn_destroy(ctx);
        return false;
    }

    ret = rknn_outputs_get(ctx, io_num.n_output, outputs.data(), nullptr);
    if (ret < 0) {
        err = QStringLiteral("获取输出失败, 错误码: %1").arg(ret);
        rknn_destroy(ctx);
        return false;
    }

    const float scale_w = static_cast<float>(width) / static_cast<float>(img_width);
    const float scale_h = static_cast<float>(height) / static_cast<float>(img_height);

    DetectResultGroup result_group{};
    std::vector<float> out_scales;
    std::vector<int32_t> out_zps;
    out_scales.reserve(io_num.n_output);
    out_zps.reserve(io_num.n_output);

    for (uint32_t i = 0; i < io_num.n_output; ++i) {
        out_scales.push_back(output_attrs[i].scale);
        out_zps.push_back(output_attrs[i].zp);
    }

    ret = post_process(static_cast<int8_t *>(outputs[0].buf),
                       static_cast<int8_t *>(outputs[1].buf),
                       static_cast<int8_t *>(outputs[2].buf),
                       height,
                       width,
                       BOX_THRESH,
                       NMS_THRESH,
                       scale_w,
                       scale_h,
                       out_zps,
                       out_scales,
                       &result_group);
    if (ret < 0) {
        err = QStringLiteral("后处理失败");
        rknn_outputs_release(ctx, io_num.n_output, outputs.data());
        rknn_destroy(ctx);
        return false;
    }

    auto end = std::chrono::steady_clock::now();
    const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    printf("inference time: %lld ms\n", static_cast<long long>(duration_ms));

    for (int i = 0; i < result_group.count; ++i) {
        const DetectResult &det = result_group.results[i];
        cv::rectangle(orig_img,
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
        if (x + label_size.width > orig_img.cols) x = orig_img.cols - label_size.width;

        cv::rectangle(orig_img,
                      cv::Rect(cv::Point(x, y), cv::Size(label_size.width, label_size.height + baseLine)),
                      cv::Scalar(255, 255, 255),
                      cv::FILLED);
        cv::putText(orig_img,
                    text,
                    cv::Point(x, y + label_size.height),
                    cv::FONT_HERSHEY_SIMPLEX,
                    0.5,
                    cv::Scalar(0, 0, 0),
                    1);
    }

    rknn_outputs_release(ctx, io_num.n_output, outputs.data());
    rknn_destroy(ctx);

    if (!cv::imwrite(outputImagePath.toStdString(), orig_img)) {
        err = QStringLiteral("写出检测结果失败: %1").arg(outputImagePath);
        return false;
    }

    qInfo().noquote() << QStringLiteral("NPU 检测输出: %1 (共 %2 个目标)")
                             .arg(outputImagePath)
                             .arg(result_group.count);
    return true;
}
