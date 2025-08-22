#include "MsgQueue.h"
#include "data.h"
#include "encoder.h"
#include "pw-stream.h"
#include "wav_file.h"

#include <csignal>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <thread>

#define DECODER_DEBUGGER 1

static pthread_t pw_thread;

static void sigint_handler(int) {
  std::cerr << "Main caught Ctrl-C, forwarding SIGINT to PipeWire thread\n";
  pthread_kill(pw_thread, SIGINT);
}

static void install_sig_handler() {
  // Install handler in main that forwards SIGINT
  struct sigaction sa{};
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, nullptr);
}

int main() {
  MsgQueue<PcmData> pcm_queue(64);
  MsgQueue<Codec2Data> codec2_queue(64);

  install_sig_handler();

  std::thread pw_worker([&pcm_queue] {
    try {
      pw_thread = pthread_self();
      PwStream pw_stream(&pcm_queue);
      pw_stream.run();
    } catch (const std::exception &ex) {
      std::cerr << "Error: " << ex.what() << "\n";
    }

    std::cout << "pw_worker Finished" << std::endl;
  });

  std::thread encoder_worker([&pcm_queue, &codec2_queue] {
    Encoder encoder = Encoder();
    while (auto maybe_data = pcm_queue.recv()) {
      PcmData data = std::move(*maybe_data);

      Codec2Data codec2_data = encoder.encode(data);

      codec2_queue.send(codec2_data);

      std::cout << "encoder " << data.session_id << "." << data.piece_id;
      std::cout << " :: ";
      std::cout << std::hex << std::setw(2) << std::setfill('0');

      for (size_t i = 0; i < CODEC2_FRAME_MAX; ++i) {
        std::cout << static_cast<int>(codec2_data.bytes[i]) << " ";
      }

      std::cout << std::dec;
      std::cout << std::endl;
    }

    std::cout << "encoder_worker Finished" << std::endl;
    std::cout << "Closing codec2_queue" << std::endl;

    codec2_queue.close();
  });

#ifdef DECODER_DEBUGGER
  std::thread decoder_debugger([&codec2_queue] {
    Encoder encoder = Encoder();
    WavFile wav_file = WavFile("recording.wav");

    while (auto maybe_data = codec2_queue.recv()) {
      Codec2Data data = std::move(*maybe_data);

      PcmData pcm_data = encoder.decode(data);

      wav_file.write_pcm(pcm_data);
    }
  });
#endif

  // std::thread lora_sender([&codec2_queue] {
  // });

  std::cout << "Wait for pw_worker" << std::endl;
  pw_worker.join();

  std::cout << "Wait for encoder_worker" << std::endl;
  encoder_worker.join();

#ifdef DECODER_DEBUGGER
  std::cout << "Wait for decoder_debugger" << std::endl;
  decoder_debugger.join();
#endif

  return 0;
}
