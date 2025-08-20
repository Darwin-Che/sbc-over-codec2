#pragma once

#include "SPSCQueue.h"
#include "data.h"

class PwStreamImpl;

class PwStream {
public:
  explicit PwStream(rigtorp::SPSCQueue<PcmData> *pcm_queue);
  ~PwStream();

  void run();

private:
  std::unique_ptr<PwStreamImpl> impl_;
};