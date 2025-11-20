#ifndef RK3566_CHATROOM_YOLOV5RUNNER_H
#define RK3566_CHATROOM_YOLOV5RUNNER_H

#include <QString>
#include <QtGlobal>

#include "npu/postprocess.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <rknn_api.h>

namespace cv {
class Mat;
}

class YoloV5Runner {
public:
    struct InferenceBreakdown {
        qint64 preprocessUs = -1;
        qint64 npuUs = -1;
        qint64 postprocessUs = -1;
    };

    struct InputTensorView {
        uint8_t *data = nullptr;
        int stride = 0;
        int dmaFd = -1;

        bool isValid() const { return data != nullptr && stride > 0; }
    };

    YoloV5Runner();
    ~YoloV5Runner();

    bool loadModel(const QString &modelPath, QString *errorOut);
    bool isReady() const;
    int inputWidth() const;
    int inputHeight() const;
    InputTensorView inputTensorView() const;

    bool infer(const cv::Mat &frameBgr,
               DetectResultGroup *resultOut,
               std::int64_t *inferenceTimeMs,
               cv::Mat *visualizedFrame,
               QString *errorOut,
               InferenceBreakdown *breakdownOut = nullptr);

    bool inferFromRgbBuffer(const cv::Mat &originalFrameBgr,
                            int originalWidth,
                            int originalHeight,
                            const uint8_t *rgbData,
                            int rgbStride,
                            const LetterboxTransform &letterbox,
                            DetectResultGroup *resultOut,
                            std::int64_t *inferenceTimeMs,
                            cv::Mat *visualizedFrame,
                            QString *errorOut,
                            InferenceBreakdown *breakdownOut = nullptr);

    static bool runSample(const QString &modelPath,
                          const QString &inputImagePath,
                          const QString &outputImagePath,
                          QString *errorOut);

private:
    void release();
    bool runInferenceInternal(const cv::Mat &originalFrameBgr,
                              int originalWidth,
                              int originalHeight,
                              const uint8_t *inputBuffer,
                              bool inputMatchesBoundMemory,
                              const LetterboxTransform &letterbox,
                              QString &err,
                              DetectResultGroup *group,
                              std::int64_t *inferenceTimeMs,
                              cv::Mat *visualizedFrame,
                              InferenceBreakdown *breakdown);
    bool initializeInputMemory(QString &err);
    void releaseInputMemory();
    std::size_t inputBufferBytes() const;

    rknn_context ctx_;
    bool ready_;
    int inputWidth_;
    int inputHeight_;
    int inputChannel_;
    rknn_input_output_num ioNum_;
    std::vector<rknn_tensor_attr> inputAttrs_;
    std::vector<rknn_tensor_attr> outputAttrs_;
    std::vector<float> outputScales_;
    std::vector<int32_t> outputZps_;
    rknn_tensor_mem *inputMem_;
    InputTensorView inputTensorView_;
};

#endif
