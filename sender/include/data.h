#pragma once

#include <cstdint>
#include <cstddef>

#define PCM_SAMPLE_MAX 320

#define CODEC2_FRAME_MAX 4

struct PcmData {
    uint32_t session_id;
    uint32_t piece_id;
    uint32_t samples_n;
    int16_t samples[PCM_SAMPLE_MAX];
};

struct Codec2Data {
    uint8_t bytes[CODEC2_FRAME_MAX];
};
