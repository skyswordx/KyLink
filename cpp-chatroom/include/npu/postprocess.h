// Adapted from Rockchip RKNN YOLOv5 sample code (Apache License 2.0).

#ifndef RK3566_CHATROOM_POSTPROCESS_H
#define RK3566_CHATROOM_POSTPROCESS_H

#include <cstdint>
#include <vector>

#include "npu/InferenceGeometry.h"

#define OBJ_NAME_MAX_SIZE 16
#define OBJ_NUMB_MAX_SIZE 64
#define OBJ_CLASS_NUM 80
#define NMS_THRESH 0.45f
#define BOX_THRESH 0.25f
#define PROP_BOX_SIZE (5 + OBJ_CLASS_NUM)

struct BoxRect {
    int left;
    int right;
    int top;
    int bottom;
};

struct DetectResult {
    char name[OBJ_NAME_MAX_SIZE];
    BoxRect box;
    float prop;
};

struct DetectResultGroup {
    int id;
    int count;
    DetectResult results[OBJ_NUMB_MAX_SIZE];
};

int post_process(int8_t *input0,
                 int8_t *input1,
                 int8_t *input2,
                 int model_in_h,
                 int model_in_w,
                 int original_img_h,
                 int original_img_w,
                 float conf_threshold,
                 float nms_threshold,
                 const LetterboxTransform &letterbox,
                 std::vector<int32_t> &qnt_zps,
                 std::vector<float> &qnt_scales,
                 DetectResultGroup *group);

#endif
