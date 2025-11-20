// Adapted from Rockchip RKNN YOLOv5 sample code (Apache License 2.0).

#include "npu/postprocess.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

namespace {

constexpr const char *kLabels[] = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat", "traffic light",
    "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow",
    "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee",
    "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard",
    "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch",
    "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone",
    "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear",
    "hair drier", "toothbrush"};

constexpr int kAnchor0[6] = {10, 13, 16, 30, 33, 23};
constexpr int kAnchor1[6] = {30, 61, 62, 45, 59, 119};
constexpr int kAnchor2[6] = {116, 90, 156, 198, 373, 326};

inline int clamp_int(float val, int min, int max) {
    return val > min ? (val < max ? static_cast<int>(val) : max) : min;
}

float calculate_overlap(float xmin0,
                        float ymin0,
                        float xmax0,
                        float ymax0,
                        float xmin1,
                        float ymin1,
                        float xmax1,
                        float ymax1) {
    float w = std::max(0.f, std::min(xmax0, xmax1) - std::max(xmin0, xmin1) + 1.0f);
    float h = std::max(0.f, std::min(ymax0, ymax1) - std::max(ymin0, ymin1) + 1.0f);
    float intersection = w * h;
    float union_area = (xmax0 - xmin0 + 1.0f) * (ymax0 - ymin0 + 1.0f) +
                       (xmax1 - xmin1 + 1.0f) * (ymax1 - ymin1 + 1.0f) - intersection;
    return union_area <= 0.f ? 0.f : (intersection / union_area);
}

int non_max_suppression(int valid_count,
                        std::vector<float> &output_locations,
                        std::vector<int> &class_ids,
                        std::vector<int> &order,
                        int filter_id,
                        float threshold) {
    for (int i = 0; i < valid_count; ++i) {
        if (order[i] == -1 || class_ids[i] != filter_id) {
            continue;
        }
        int n = order[i];
        for (int j = i + 1; j < valid_count; ++j) {
            int m = order[j];
            if (m == -1 || class_ids[j] != filter_id) {
                continue;
            }
            float xmin0 = output_locations[n * 4 + 0];
            float ymin0 = output_locations[n * 4 + 1];
            float xmax0 = output_locations[n * 4 + 0] + output_locations[n * 4 + 2];
            float ymax0 = output_locations[n * 4 + 1] + output_locations[n * 4 + 3];

            float xmin1 = output_locations[m * 4 + 0];
            float ymin1 = output_locations[m * 4 + 1];
            float xmax1 = output_locations[m * 4 + 0] + output_locations[m * 4 + 2];
            float ymax1 = output_locations[m * 4 + 1] + output_locations[m * 4 + 3];

            float iou = calculate_overlap(xmin0, ymin0, xmax0, ymax0, xmin1, ymin1, xmax1, ymax1);

            if (iou > threshold) {
                order[j] = -1;
            }
        }
    }
    return 0;
}

int quick_sort_indices_inverse(std::vector<float> &input,
                               int left,
                               int right,
                               std::vector<int> &indices) {
    int low = left;
    int high = right;
    if (left < right) {
        int key_index = indices[left];
        float key = input[left];
        while (low < high) {
            while (low < high && input[high] <= key) {
                --high;
            }
            input[low] = input[high];
            indices[low] = indices[high];
            while (low < high && input[low] >= key) {
                ++low;
            }
            input[high] = input[low];
            indices[high] = indices[low];
        }
        input[low] = key;
        indices[low] = key_index;
        quick_sort_indices_inverse(input, left, low - 1, indices);
        quick_sort_indices_inverse(input, low + 1, right, indices);
    }
    return low;
}

inline int32_t clip(float val, float min, float max) {
    return static_cast<int32_t>(val <= min ? min : (val >= max ? max : val));
}

int8_t quantize_to_affine(float value, int32_t zero_point, float scale) {
    float dst = (value / scale) + zero_point;
    return static_cast<int8_t>(clip(dst, -128.0f, 127.0f));
}

float dequantize_from_affine(int8_t value, int32_t zero_point, float scale) {
    return (static_cast<float>(value) - static_cast<float>(zero_point)) * scale;
}

int process_tensor(int8_t *input,
                   const int *anchor,
                   int grid_h,
                   int grid_w,
                   int model_h,
                   int model_w,
                   int stride,
                   std::vector<float> &boxes,
                   std::vector<float> &object_probs,
                   std::vector<int> &class_ids,
                   float threshold,
                   int32_t zero_point,
                   float scale) {
    int valid_count = 0;
    int grid_len = grid_h * grid_w;
    int8_t threshold_i8 = quantize_to_affine(threshold, zero_point, scale);

    for (int a = 0; a < 3; ++a) {
        for (int i = 0; i < grid_h; ++i) {
            for (int j = 0; j < grid_w; ++j) {
                int8_t box_conf = input[(PROP_BOX_SIZE * a + 4) * grid_len + i * grid_w + j];
                if (box_conf >= threshold_i8) {
                    int offset = (PROP_BOX_SIZE * a) * grid_len + i * grid_w + j;
                    int8_t *in_ptr = input + offset;
                    float box_x = (dequantize_from_affine(*in_ptr, zero_point, scale)) * 2.f - 0.5f;
                    float box_y = (dequantize_from_affine(in_ptr[grid_len], zero_point, scale)) * 2.f - 0.5f;
                    float box_w = (dequantize_from_affine(in_ptr[2 * grid_len], zero_point, scale)) * 2.f;
                    float box_h = (dequantize_from_affine(in_ptr[3 * grid_len], zero_point, scale)) * 2.f;
                    box_x = (box_x + j) * static_cast<float>(stride);
                    box_y = (box_y + i) * static_cast<float>(stride);
                    box_w = box_w * box_w * static_cast<float>(anchor[a * 2]);
                    box_h = box_h * box_h * static_cast<float>(anchor[a * 2 + 1]);
                    box_x -= (box_w / 2.0f);
                    box_y -= (box_h / 2.0f);

                    int8_t max_class_prob = in_ptr[5 * grid_len];
                    int max_class_id = 0;
                    for (int k = 1; k < OBJ_CLASS_NUM; ++k) {
                        int8_t current = in_ptr[(5 + k) * grid_len];
                        if (current > max_class_prob) {
                            max_class_prob = current;
                            max_class_id = k;
                        }
                    }

                    if (max_class_prob > threshold_i8) {
                        object_probs.push_back(dequantize_from_affine(max_class_prob, zero_point, scale) *
                                               dequantize_from_affine(box_conf, zero_point, scale));
                        class_ids.push_back(max_class_id);
                        ++valid_count;
                        boxes.push_back(box_x);
                        boxes.push_back(box_y);
                        boxes.push_back(box_w);
                        boxes.push_back(box_h);
                    }
                }
            }
        }
    }

    return valid_count;
}

}  // namespace

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
                 DetectResultGroup *group) {
    if (!group) {
        return -1;
    }

    std::memset(group, 0, sizeof(DetectResultGroup));

    std::vector<float> filter_boxes;
    std::vector<float> object_probs;
    std::vector<int> class_ids;

    const int stride0 = 8;
    const int grid_h0 = model_in_h / stride0;
    const int grid_w0 = model_in_w / stride0;
    const int valid_count0 = process_tensor(input0,
                                            kAnchor0,
                                            grid_h0,
                                            grid_w0,
                                            model_in_h,
                                            model_in_w,
                                            stride0,
                                            filter_boxes,
                                            object_probs,
                                            class_ids,
                                            conf_threshold,
                                            qnt_zps[0],
                                            qnt_scales[0]);

    const int stride1 = 16;
    const int grid_h1 = model_in_h / stride1;
    const int grid_w1 = model_in_w / stride1;
    const int valid_count1 = process_tensor(input1,
                                            kAnchor1,
                                            grid_h1,
                                            grid_w1,
                                            model_in_h,
                                            model_in_w,
                                            stride1,
                                            filter_boxes,
                                            object_probs,
                                            class_ids,
                                            conf_threshold,
                                            qnt_zps[1],
                                            qnt_scales[1]);

    const int stride2 = 32;
    const int grid_h2 = model_in_h / stride2;
    const int grid_w2 = model_in_w / stride2;
    const int valid_count2 = process_tensor(input2,
                                            kAnchor2,
                                            grid_h2,
                                            grid_w2,
                                            model_in_h,
                                            model_in_w,
                                            stride2,
                                            filter_boxes,
                                            object_probs,
                                            class_ids,
                                            conf_threshold,
                                            qnt_zps[2],
                                            qnt_scales[2]);

    const int valid_count = valid_count0 + valid_count1 + valid_count2;
    if (valid_count <= 0) {
        return 0;
    }

    std::vector<int> index_array;
    index_array.reserve(valid_count);
    for (int i = 0; i < valid_count; ++i) {
        index_array.push_back(i);
    }

    quick_sort_indices_inverse(object_probs, 0, valid_count - 1, index_array);

    std::set<int> class_set(class_ids.begin(), class_ids.end());
    for (int cls : class_set) {
        non_max_suppression(valid_count, filter_boxes, class_ids, index_array, cls, nms_threshold);
    }

    int last_count = 0;
    group->count = 0;
    const float fallbackInvScaleW = (model_in_w > 0 && original_img_w > 0)
                                        ? static_cast<float>(original_img_w) / static_cast<float>(model_in_w)
                                        : 1.f;
    const float fallbackInvScaleH = (model_in_h > 0 && original_img_h > 0)
                                        ? static_cast<float>(original_img_h) / static_cast<float>(model_in_h)
                                        : 1.f;

    auto convertCoord = [&](float value, bool isX) {
        if (letterbox.isValid() && letterbox.scaleX > 0.f && letterbox.scaleY > 0.f) {
            const float scale = isX ? letterbox.scaleX : letterbox.scaleY;
            const float pad = isX ? static_cast<float>(letterbox.padLeft)
                                  : static_cast<float>(letterbox.padTop);
            const float unpadded = (value - pad) / scale;
            return clamp_int(unpadded,
                             0,
                             isX ? original_img_w : original_img_h);
        }
        const float fallback = isX ? fallbackInvScaleW : fallbackInvScaleH;
        return clamp_int(value * fallback,
                         0,
                         isX ? original_img_w : original_img_h);
    };

    for (int i = 0; i < valid_count; ++i) {
        if (index_array[i] == -1 || last_count >= OBJ_NUMB_MAX_SIZE) {
            continue;
        }
        int n = index_array[i];
        float x1 = filter_boxes[n * 4 + 0];
        float y1 = filter_boxes[n * 4 + 1];
        float x2 = x1 + filter_boxes[n * 4 + 2];
        float y2 = y1 + filter_boxes[n * 4 + 3];
        int cls = class_ids[n];
        float obj_conf = object_probs[i];

        group->results[last_count].box.left = convertCoord(x1, true);
        group->results[last_count].box.top = convertCoord(y1, false);
        group->results[last_count].box.right = convertCoord(x2, true);
        group->results[last_count].box.bottom = convertCoord(y2, false);
        group->results[last_count].prop = obj_conf;
        std::strncpy(group->results[last_count].name, kLabels[cls], OBJ_NAME_MAX_SIZE - 1);
        group->results[last_count].name[OBJ_NAME_MAX_SIZE - 1] = '\0';
        ++last_count;
    }

    group->count = last_count;
    return 0;
}
