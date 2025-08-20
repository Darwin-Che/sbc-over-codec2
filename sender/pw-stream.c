#define _GNU_SOURCE
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/format-utils.h>
#include <spa/utils/defs.h>
#include <spa/utils/result.h>

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Context {
  struct pw_main_loop *loop;
  // struct pw_context     *context;
  // struct pw_core        *core;
  // struct pw_registry    *registry;

  // int                    connected;

  struct pw_stream *stream;

  struct spa_audio_info format;
};

static void on_process(void *context) {
  struct context *ctx = context;
  struct pw_buffer *b;
  struct spa_buffer *buf;

  int16_t *samples, max;
  uint32_t i, n_samples;

  if ((b = pw_stream_dequeue_buffer(ctx->stream)) == NULL) {
    pw_log_warn("out of buffers: %m");
    return;
  }

  buf = b->buffer;
  if ((samples = buf->datas[0].data) == NULL)
    return;

  n_samples = buf->datas[0].chunk->size / sizeof(int16_t);

  fprintf(stdout, "%c[%dA", 0x1b, 2);

  fprintf(stdout, "captured %d samples\n", n_samples);

  max = 0;

  for (i = 0; i < n_samples; ++i) {
    if (samples[i] > max)
      max = samples[i];
  }

  // fprintf(stdout, "|%*s| peak:%f\n", max + 1, "*", max);
  fprintf(stdout, "peak:%d\n", max);

  fflush(stdout);

  pw_stream_queue_buffer(ctx->stream, b);
}

static void on_stream_param_changed(void *_data, uint32_t id,
                                    const struct spa_pod *param) {
  struct context *ctx = _data;

  /* NULL means to clear the format */
  if (param == NULL || id != SPA_PARAM_Format)
    return;

  if (spa_format_parse(param, &ctx->format.media_type,
                       &ctx->format.media_subtype) < 0)
    return;

  /* only accept raw audio */
  if (ctx->format.media_type != SPA_MEDIA_TYPE_audio ||
      ctx->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
    return;

  /* call a helper function to parse the format for us. */
  spa_format_audio_raw_parse(param, &ctx->format.info.raw);

  fprintf(stdout, "capturing rate:%d channels:%d\n", ctx->format.info.raw.rate,
          ctx->format.info.raw.channels);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *context, int signal_number) {
  struct context *ctx = context;
  pw_main_loop_quit(ctx->loop);
}

int main(int argc, char *argv[]) {
  struct context ctx = {0};

  const struct spa_pod *params[1];
  uint8_t buffer[1024];
  struct pw_properties *props;
  struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

  pw_init(&argc, &argv);

  ctx.loop = pw_main_loop_new(NULL);

  pw_loop_add_signal(pw_main_loop_get_loop(ctx.loop), SIGINT, do_quit, &ctx);
  pw_loop_add_signal(pw_main_loop_get_loop(ctx.loop), SIGTERM, do_quit, &ctx);

  props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
                            "Capture", PW_KEY_MEDIA_ROLE, "Music", NULL);

  ctx.stream =
      pw_stream_new_simple(pw_main_loop_get_loop(ctx.loop), "audio-capture",
                           props, &stream_events, &ctx);

  params[0] = spa_format_audio_raw_build(
      &b, SPA_PARAM_EnumFormat,
      &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16_LE, .rate = 8000,
                               .channels = 1,
                               .position = {SPA_AUDIO_CHANNEL_MONO}));

  /* Now connect this stream. We ask that our process function is
   * called in a realtime thread. */
  pw_stream_connect(ctx.stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                    PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                        PW_STREAM_FLAG_RT_PROCESS,
                    params, 1);

  /* and wait while we let things run */
  pw_main_loop_run(ctx.loop);

  pw_stream_destroy(ctx.stream);
  pw_main_loop_destroy(ctx.loop);
  pw_deinit();

  return 0;
}