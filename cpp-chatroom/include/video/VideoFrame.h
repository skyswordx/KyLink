#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include <cstdint>
#include <vector>
#include <string>

/**
 * @brief 视频帧UDP包头结构
 * 
 * 用于UDP分包传输JPEG视频帧。
 * 单个JPEG帧(~50KB)被拆分为多个UDP包，每个包携带此头信息。
 */
#pragma pack(push, 1)
struct VideoChunkHeader {
    uint32_t magic;         // VIDEO_FRAME_MAGIC (0x56464551)
    uint32_t frameId;       // 帧ID，递增
    uint16_t chunkIndex;    // 当前分片索引 (0-based)
    uint16_t totalChunks;   // 总分片数
    uint32_t payloadSize;   // 本包载荷大小
};
#pragma pack(pop)

/**
 * @brief 一个完整视频帧的元数据
 */
struct VideoFrameMeta {
    uint32_t frameId = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint64_t timestamp = 0; // 毫秒时间戳
};

/**
 * @brief 视频帧数据包
 */
struct VideoFramePacket {
    VideoFrameMeta meta;
    std::vector<uint8_t> jpegData;
    
    bool isValid() const { return !jpegData.empty(); }
};

/**
 * @brief 将JPEG帧数据拆分为UDP分包
 * 
 * @param frameId 帧ID
 * @param jpegData JPEG数据
 * @param maxChunkSize 单包最大载荷
 * @return 分包后的UDP数据包列表
 */
std::vector<std::vector<uint8_t>> splitFrameToChunks(
    uint32_t frameId,
    const std::vector<uint8_t>& jpegData,
    size_t maxChunkSize = 1400
);

/**
 * @brief 从UDP包解析视频帧头
 * 
 * @param data UDP数据
 * @param size 数据大小
 * @param header 输出头信息
 * @return 解析成功返回true
 */
bool parseChunkHeader(const uint8_t* data, size_t size, VideoChunkHeader& header);

#endif // VIDEOFRAME_H
