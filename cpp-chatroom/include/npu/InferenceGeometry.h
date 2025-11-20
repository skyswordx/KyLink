#ifndef RK3566_CHATROOM_INFERENCE_GEOMETRY_H
#define RK3566_CHATROOM_INFERENCE_GEOMETRY_H

struct LetterboxTransform {
    float scaleX = 1.f;
    float scaleY = 1.f;
    int padLeft = 0;
    int padTop = 0;
    int contentWidth = 0;
    int contentHeight = 0;

    bool isValid() const {
        return scaleX > 0.f && scaleY > 0.f && contentWidth >= 0 && contentHeight >= 0;
    }
};

#endif  // RK3566_CHATROOM_INFERENCE_GEOMETRY_H
