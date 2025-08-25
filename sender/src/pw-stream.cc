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

#include "MsgQueue.h"
#include "data.h"
#include "pw-stream.h"

#define PW_STREAM_DEBUG 1

#ifdef PW_STREAM_DEBUG
#include "wav_file.h"
#endif

class PwStreamImpl {
public:
  PwStreamImpl(MsgQueue<PcmData> *pcm_queue) : pcm_queue(pcm_queue) {
    pw_init(nullptr, nullptr);

    loop = pw_main_loop_new(nullptr);
    if (!loop) {
      throw std::runtime_error("Failed to create PipeWire main loop");
    }

    // Register signal handlers
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGINT,
                       &PwStreamImpl::do_quit, this);
    pw_loop_add_signal(pw_main_loop_get_loop(loop), SIGTERM,
                       &PwStreamImpl::do_quit, this);

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

#ifdef PW_STREAM_DEBUG
    wav_file = new WavFile("pw-stream-debug.wav");
#endif
  }

  ~PwStreamImpl() {
    if (stream)
      pw_stream_destroy(stream);
    if (loop)
      pw_main_loop_destroy(loop);
    pw_deinit();

#ifdef PW_STREAM_DEBUG
    delete wav_file;
#endif
  }

  void run() { pw_main_loop_run(loop); }

  void reset_session() {
    this->new_session = true;

    this->pcm_data.session_id += 1;
    this->pcm_data.piece_id = 0;
    this->pcm_data.samples_n = 0;

    this->zero_samples = 0;
  }

  void send_data(int16_t *samples, uint32_t n_samples) {
    while (n_samples != 0) {
      uint32_t copy_amount = PCM_SAMPLE_MAX - this->pcm_data.samples_n;

      if (copy_amount > n_samples)
        copy_amount = n_samples;

      memcpy(&this->pcm_data.samples[this->pcm_data.samples_n], samples,
             copy_amount * sizeof(int16_t));

      n_samples -= copy_amount;
      samples += copy_amount;

      this->pcm_data.samples_n += copy_amount;

      if (this->pcm_data.samples_n == PCM_SAMPLE_MAX)
        this->emit_pcm_data();
    }
  }

  void emit_pcm_data() {
    // std::cout << "emit_pcm_data " << this->pcm_data.session_id << "."
    //           << this->pcm_data.piece_id << std::endl;

    this->pcm_queue->send(this->pcm_data);

#ifdef PW_STREAM_DEBUG
    this->wav_file->write_pcm(this->pcm_data);
#endif

    this->pcm_data.piece_id += 1;
    this->pcm_data.samples_n = 0;
  }

private:
  struct pw_main_loop *loop = nullptr;
  struct pw_stream *stream = nullptr;
  struct spa_audio_info format = {};

  MsgQueue<PcmData> *pcm_queue;

  PcmData pcm_data = {0};

  uint64_t zero_samples = 0;

  bool new_session = true;

#ifdef PW_STREAM_DEBUG
  WavFile *wav_file;
#endif

  static void do_quit(void *data, int) {

    auto *ctx = static_cast<PwStreamImpl *>(data);

    ctx->pcm_queue->close();

    std::cout << "Closed pcm_queue" << std::endl;

    pw_main_loop_quit(ctx->loop);

    std::cout << "PwStream Do Quit Processed" << std::endl;
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
    // std::cout << "\033[2A";

    // std::cout << "captured " << n_samples << " samples\n";

    // int16_t max = 0;
    // for (uint32_t i = 0; i < n_samples; ++i) {
    //   if (samples[i] > max)
    //     max = samples[i];
    // }

    // std::cout << "peak:" << max << "\n" << std::flush;

    /////// Actual process

    // Session change check: more than 1 sec of silence
    // New session, only start sending if non zero data comes in
    // Always starts with the first non zero sample

    // Whenever PCM_SAMPLE_MAX samples are collected, send an object on the
    // queue Signal via conditional variable

    // Make a wrapper for the queue with conditonal variables and sample

    uint32_t idx_first_nonzero = 0;

    while (idx_first_nonzero < n_samples) {
      if (samples[idx_first_nonzero] != 0)
        break;
      ++idx_first_nonzero;
    }

    if (ctx->new_session) {
      if (idx_first_nonzero != n_samples) {
        ctx->new_session = false;

        ctx->send_data(&samples[idx_first_nonzero],
                       n_samples - idx_first_nonzero);
      }
    } else {
      ctx->send_data(samples, n_samples);

      if (idx_first_nonzero == n_samples) {
        ctx->zero_samples += n_samples;

        // More than 1s of silence
        if (ctx->zero_samples >= 8000) {
          // Reset the session
          ctx->reset_session();
        }
      } else {
        ctx->zero_samples = 0;
      }
    }

    pw_stream_queue_buffer(ctx->stream, b);
  }

  static constexpr pw_stream_events stream_events = {
      .version = PW_VERSION_STREAM_EVENTS,
      .param_changed = &PwStreamImpl::on_stream_param_changed,
      .process = &PwStreamImpl::on_process,
      // .destroy = nullptr,
      // .state_changed = nullptr,
      // .control_info = nullptr,
      // .io_changed = nullptr,
      // .add_buffer = nullptr,
      // .remove_buffer = nullptr,
      // .drained = nullptr,
      // .command = nullptr,
      // .trigger_done = nullptr
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

PwStream::PwStream(MsgQueue<PcmData> *pcm_queue)
    : impl_(std::make_unique<PwStreamImpl>(pcm_queue)) {}

PwStream::~PwStream() = default;

void PwStream::run() { impl_->run(); }
