#include "SPSCQueue.h"
#include "data.h"
#include "pw-stream.h"

#include <iostream>

int main() {
  rigtorp::SPSCQueue<PcmData> pcm_queue(64);

  try {
    PwStream pw_stream(&pcm_queue);
    pw_stream.run();
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}