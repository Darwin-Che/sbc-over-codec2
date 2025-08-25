#pragma once

#include <cstdint>
#include <cstddef>

#include "codec2.h"

#define CODEC2_MODE CODEC2_MODE_700C

#if CODEC2_MODE==CODEC2_MODE_700C
#define PCM_SAMPLE_MAX 320
#define CODEC2_FRAME_MAX 4
#endif

#if CODEC2_MODE==CODEC2_MODE_1300
#define PCM_SAMPLE_MAX 320
#define CODEC2_FRAME_MAX 7
#endif

#if CODEC2_MODE==CODEC2_MODE_2400
#define PCM_SAMPLE_MAX 160
#define CODEC2_FRAME_MAX 6
#endif

#if CODEC2_MODE==CODEC2_MODE_3200
#define PCM_SAMPLE_MAX 160
#define CODEC2_FRAME_MAX 8
#endif


struct PcmData {
    uint32_t session_id;
    uint32_t piece_id;
    uint32_t samples_n;
    int16_t samples[PCM_SAMPLE_MAX];
};

struct Codec2Data {
    uint8_t bytes[CODEC2_FRAME_MAX];
};
