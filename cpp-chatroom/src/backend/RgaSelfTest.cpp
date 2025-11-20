#include "backend/RgaSelfTest.h"

#include <QtCore/QDebug>

#include <array>
#include <cstring>

#ifdef HAVE_RGA
#include <rga/im2d.hpp>
#include <rga/rga.h>
#endif

namespace diagnostics {
namespace {
constexpr int kTestWidth = 64;
constexpr int kTestHeight = 64;
constexpr int kChannels = 4; // RGBA8888
}

bool runRgaSelfTest(QString* errorOut) {
#ifndef HAVE_RGA
    if (errorOut) {
        *errorOut = QStringLiteral("当前构建未启用 RGA，自检被跳过");
    }
    return true;
#else
    std::array<uint8_t, kTestWidth * kTestHeight * kChannels> srcBuffer{};
    std::array<uint8_t, kTestWidth * kTestHeight * kChannels> dstBuffer{};

    for (int y = 0; y < kTestHeight; ++y) {
        for (int x = 0; x < kTestWidth; ++x) {
            const int offset = (y * kTestWidth + x) * kChannels;
            srcBuffer[static_cast<std::size_t>(offset + 0)] = static_cast<uint8_t>((x * 255) / kTestWidth); // R
            srcBuffer[static_cast<std::size_t>(offset + 1)] = static_cast<uint8_t>((y * 255) / kTestHeight); // G
            srcBuffer[static_cast<std::size_t>(offset + 2)] = static_cast<uint8_t>(((x + y) * 255) / (kTestWidth + kTestHeight)); // B
            srcBuffer[static_cast<std::size_t>(offset + 3)] = 0xFF; // A
        }
    }

    rga_buffer_t src = wrapbuffer_virtualaddr(srcBuffer.data(), kTestWidth, kTestHeight, RK_FORMAT_RGBA_8888);
    rga_buffer_t dst = wrapbuffer_virtualaddr(dstBuffer.data(), kTestWidth, kTestHeight, RK_FORMAT_RGBA_8888);

    const im_rect srcRect{0, 0, kTestWidth, kTestHeight};
    const im_rect dstRect{0, 0, kTestWidth, kTestHeight};
    const rga_buffer_t pat{};
    const im_rect patRect{0, 0, 0, 0};

    IM_STATUS status = imcheck_t(src, dst, pat, srcRect, dstRect, patRect, 0);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        if (errorOut) {
            *errorOut = QStringLiteral("RGA 参数校验失败: %1").arg(QString::fromUtf8(imStrError(status)));
        }
        return false;
    }

    status = imcopy(src, dst);
    if (status != IM_STATUS_SUCCESS && status != IM_STATUS_NOERROR) {
        if (errorOut) {
            *errorOut = QStringLiteral("RGA imcopy 执行失败: %1").arg(QString::fromUtf8(imStrError(status)));
        }
        return false;
    }

    if (std::memcmp(srcBuffer.data(), dstBuffer.data(), dstBuffer.size()) != 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("RGA imcopy 输出与输入不一致");
        }
        return false;
    }

    qInfo().noquote() << QStringLiteral("RGA 自检通过，驱动版本: %1")
                             .arg(QString::fromUtf8(querystring(RGA_VERSION)));
    return true;
#endif
}

} // namespace diagnostics
