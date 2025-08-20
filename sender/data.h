#pragma once

#define PCM_SAMPLE_MAX 160

struct PcmData {
    uint32_t session_id;
    uint32_t piece_id;
    uint32_t samples_n;
    int16_t samples[PCM_SAMPLE_MAX];
};

