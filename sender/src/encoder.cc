#include "encoder.h"

#include <cassert>
#include <iostream>

Encoder::Encoder() {
  codec2 = codec2_create(CODEC2_MODE_700C);
  size_t nsam = codec2_samples_per_frame(codec2);
  assert(nsam == PCM_SAMPLE_MAX);
  size_t bytes_per_frame = codec2_bytes_per_frame(codec2);
  std::cout << "bytes_per_frame: " << bytes_per_frame << std::endl;
  assert(bytes_per_frame == CODEC2_FRAME_MAX);
}

Encoder::~Encoder() { codec2_destroy(codec2); }

Codec2Data Encoder::encode(PcmData &pcm_data) {
  Codec2Data compressed_frame;

  assert(pcm_data.samples_n == PCM_SAMPLE_MAX);

  codec2_encode(this->codec2, compressed_frame.bytes, pcm_data.samples);

  return compressed_frame;
}

PcmData Encoder::decode(Codec2Data &codec2_data) {
  PcmData pcm_data;

  codec2_decode(this->codec2, pcm_data.samples, codec2_data.bytes);

  pcm_data.samples_n = PCM_SAMPLE_MAX;
  pcm_data.session_id = 0;
  pcm_data.piece_id = 0;

  return pcm_data;
}
