#pragma once

#include "MsgQueue.h"
#include "data.h"

class PwStreamImpl;

class PwStream {
public:
  explicit PwStream(MsgQueue<PcmData> *pcm_queue);
  ~PwStream();

  void run();

private:
  std::unique_ptr<PwStreamImpl> impl_;
};