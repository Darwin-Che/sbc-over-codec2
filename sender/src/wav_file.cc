#include "wav_file.h"

struct WavHeader {
  char riff[4] = {'R', 'I', 'F', 'F'};
  uint32_t chunk_size = 0; // placeholder
  char wave[4] = {'W', 'A', 'V', 'E'};
  char fmt[4] = {'f', 'm', 't', ' '};
  uint32_t subchunk1_size = 16;
  uint16_t audio_format = 1; // PCM
  uint16_t num_channels = 1;
  uint32_t sample_rate = 8000;
  uint32_t byte_rate = 0;   // placeholder
  uint16_t block_align = 0; // placeholder
  uint16_t bits_per_sample = 16;
  char data[4] = {'d', 'a', 't', 'a'};
  uint32_t subchunk2_size = 0; // placeholder
};

static WavHeader standard_header() {
  WavHeader header;

  header.block_align = header.num_channels * header.bits_per_sample / 8;
  header.byte_rate = header.block_align * header.sample_rate;

  return header;
}

WavFile::WavFile(std::string filename)
    : fs(filename, std::ios::binary), header(new WavHeader()), n_samples(0) {
  *header = standard_header();
  fs.write(reinterpret_cast<const char *>(header), sizeof(WavHeader));
}

WavFile::~WavFile() {
  uint32_t data_size = sizeof(int16_t) * n_samples;
  header->subchunk2_size = data_size;
  header->chunk_size = 36 + data_size;

  std::cout << "Wav File destructor is called, updating the headers: samples = "
            << n_samples << std::endl;

  fs.seekp(0, std::ios::beg);
  fs.write(reinterpret_cast<const char *>(header), sizeof(WavHeader));
  fs.close();
}

void WavFile::write_pcm(const PcmData &pcm_data) {
  n_samples += PCM_SAMPLE_MAX;

  fs.write(reinterpret_cast<const char *>(pcm_data.samples),
           sizeof(pcm_data.samples));
  // fs.flush();
}