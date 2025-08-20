#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/format-utils.h>
#include <spa/utils/defs.h>
#include <spa/utils/result.h>

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>

#include "SPSCQueue.h"
#include "data.h"
#include "pw-stream.h"

class PwStreamImpl {
public:
  PwStreamImpl(rigtorp::SPSCQueue<PcmData> *pcm_queue) : pcm_queue(pcm_queue) {
    pw_init(nullptr, nullptr);

    loop = pw_main_loop_new(nullptr);
    if (!loop) {
      throw std::runtime_error("Failed to create PipeWire main loop");
    }

    // Register signal handlers
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGINT, &PwStreamImpl::do_quit,
                       this);
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGTERM, &PwStreamImpl::do_quit,
                       this);

    auto *props =
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
                          "Capture", PW_KEY_MEDIA_ROLE, "Music", nullptr);

    stream = pw_stream_new_simple(pw_main_loop_get_loop(loop), "audio-capture",
                                  props, &stream_events, this);

    uint8_t buffer[1024];
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];

    spa_audio_info_raw audio_info_raw =
        SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16_LE, .rate = 8000,
                                .channels = 1,
                                .position = {SPA_AUDIO_CHANNEL_MONO});

    params[0] =
        spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audio_info_raw);

    pw_stream_connect(stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                      static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                                   PW_STREAM_FLAG_MAP_BUFFERS |
                                                   PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);
  }

  ~PwStreamImpl() {
    if (stream)
      pw_stream_destroy(stream);
    if (loop)
      pw_main_loop_destroy(loop);
    pw_deinit();
  }

  void run() { pw_main_loop_run(loop); }

private:
  struct pw_main_loop *loop = nullptr;
  struct pw_stream *stream = nullptr;
  struct spa_audio_info format = {};

  rigtorp::SPSCQueue<PcmData> *pcm_queue;

  PcmData pcm_data = {0};

  static void do_quit(void *data, int) {
    auto *ctx = static_cast<PwStreamImpl *>(data);
    pw_main_loop_quit(ctx->loop);
  }

  static void on_stream_param_changed(void *data, uint32_t id,
                                      const struct spa_pod *param) {
    auto *ctx = static_cast<PwStreamImpl *>(data);

    if (param == nullptr || id != SPA_PARAM_Format)
      return;

    if (spa_format_parse(param, &ctx->format.media_type,
                         &ctx->format.media_subtype) < 0)
      return;

    if (ctx->format.media_type != SPA_MEDIA_TYPE_audio ||
        ctx->format.media_subtype != SPA_MEDIA_SUBTYPE_raw) {
      return;
    }

    spa_format_audio_raw_parse(param, &ctx->format.info.raw);

    std::cout << "capturing rate:" << ctx->format.info.raw.rate
              << " channels:" << ctx->format.info.raw.channels << "\n";
  }

  static void on_process(void *data) {
    auto *ctx = static_cast<PwStreamImpl *>(data);

    struct pw_buffer *b = pw_stream_dequeue_buffer(ctx->stream);
    if (!b) {
      pw_log_warn("out of buffers: %m");
      return;
    }

    struct spa_buffer *buf = b->buffer;
    auto *samples = static_cast<int16_t *>(buf->datas[0].data);
    if (!samples)
      return;

    uint32_t n_samples = buf->datas[0].chunk->size / sizeof(int16_t);

    // Move cursor up 2 lines
    std::cout << "\033[2A";

    std::cout << "captured " << n_samples << " samples\n";

    int16_t max = 0;
    for (uint32_t i = 0; i < n_samples; ++i) {
      if (samples[i] > max)
        max = samples[i];
    }

    std::cout << "peak:" << max << "\n" << std::flush;

    // Actual process
    
    // Session change check: more than 1 sec of silence
    // New session, only start sending if non zero data comes in
    // Always starts with the first non zero sample

    // Whenever 160 samples are collected, send an object on the queue
    // Signal via conditional variable

    // Make a wrapper for the queue with conditonal variables and sample

    pw_stream_queue_buffer(ctx->stream, b);
  }

  static constexpr pw_stream_events stream_events = {
      PW_VERSION_STREAM_EVENTS,
      .param_changed = &PwStreamImpl::on_stream_param_changed,
      .process = &PwStreamImpl::on_process,
  };
};

// int main() {
//   try {
//     Context ctx;
//     ctx.run();
//   } catch (const std::exception &ex) {
//     std::cerr << "Error: " << ex.what() << "\n";
//     return EXIT_FAILURE;
//   }

//   return EXIT_SUCCESS;
// }


PwStream::PwStream(rigtorp::SPSCQueue<PcmData> *pcm_queue)
    : impl_(std::make_unique<PwStreamImpl>(pcm_queue)) {}

PwStream::~PwStream() = default;


void PwStream::run() {
    impl_->run();
}
