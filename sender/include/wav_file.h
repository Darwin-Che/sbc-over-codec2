#pragma once

#include <fstream>
#include <iostream>
#include <string>

#include "data.h"

struct WavHeader;

class WavFile {
public:
  WavFile(std::string filename);
  ~WavFile();

  void write_pcm(const PcmData &pcm_data);

private:
  std::ofstream fs;

  WavHeader *header;

  size_t n_samples;
};