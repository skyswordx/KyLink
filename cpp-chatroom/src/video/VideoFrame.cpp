#include "video/VideoFrame.h"
#include "ipmsg.h"
#include <cstring>

std::vector<std::vector<uint8_t>> splitFrameToChunks(
    uint32_t frameId,
    const std::vector<uint8_t>& jpegData,
    size_t maxChunkSize)
{
    std::vector<std::vector<uint8_t>> chunks;
    
    if (jpegData.empty()) {
        return chunks;
    }
    
    const size_t headerSize = sizeof(VideoChunkHeader);
    const size_t maxPayload = maxChunkSize - headerSize;
    const size_t totalSize = jpegData.size();
    const uint16_t totalChunks = static_cast<uint16_t>((totalSize + maxPayload - 1) / maxPayload);
    
    size_t offset = 0;
    for (uint16_t i = 0; i < totalChunks; ++i) {
        size_t remaining = totalSize - offset;
        size_t payloadSize = (remaining > maxPayload) ? maxPayload : remaining;
        
        // 创建包
        std::vector<uint8_t> packet(headerSize + payloadSize);
        
        // 填充头
        VideoChunkHeader* header = reinterpret_cast<VideoChunkHeader*>(packet.data());
        header->magic = VIDEO_FRAME_MAGIC;
        header->frameId = frameId;
        header->chunkIndex = i;
        header->totalChunks = totalChunks;
        header->payloadSize = static_cast<uint32_t>(payloadSize);
        
        // 填充载荷
        std::memcpy(packet.data() + headerSize, jpegData.data() + offset, payloadSize);
        
        chunks.push_back(std::move(packet));
        offset += payloadSize;
    }
    
    return chunks;
}

bool parseChunkHeader(const uint8_t* data, size_t size, VideoChunkHeader& header)
{
    if (size < sizeof(VideoChunkHeader)) {
        return false;
    }
    
    std::memcpy(&header, data, sizeof(VideoChunkHeader));
    
    if (header.magic != VIDEO_FRAME_MAGIC) {
        return false;
    }
    
    return true;
}
