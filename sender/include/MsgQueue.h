#pragma once

#include "SPSCQueue.h"
#include <optional>
#include <semaphore>

template <typename T> class MsgQueue {
public:
  MsgQueue(size_t queue_sz) : queue(rigtorp::SPSCQueue<T>(queue_sz)), sem(0) {};

  ~MsgQueue() = default;

  void close() {
    closed = true;
    sem.release();
  }

  bool send(const T &data) {
    if (closed)
      return false;

    bool result = queue.try_emplace(data);

    if (result)
      sem.release();
    return result;
  };

  std::optional<T> recv() {
    while (true) {
      if (auto ptr = queue.front()) {
        T val = std::move(*ptr);

        queue.pop();

        return val;
      }
      if (closed)
        return std::nullopt;
      sem.acquire();
    }
  };

private:
  rigtorp::SPSCQueue<T> queue;
  std::binary_semaphore sem;
  bool closed = false;
};