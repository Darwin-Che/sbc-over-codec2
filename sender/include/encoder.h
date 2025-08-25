#pragma once

// #include <codec2/codec2.h>
#include "codec2.h"
#include "data.h"

class Encoder {
public:
  Encoder();

  ~Encoder();

  Codec2Data encode(PcmData &pcm_data);

  PcmData decode(Codec2Data &codec2_data);

private:
  CODEC2 *codec2;
  size_t nsam;
  size_t bytes_per_frame;
};
