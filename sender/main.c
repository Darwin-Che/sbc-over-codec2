#define _GNU_SOURCE
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/format-utils.h>
#include <spa/utils/defs.h>
#include <spa/utils/result.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

struct app {
    struct pw_main_loop   *loop;
    struct pw_context     *context;
    struct pw_core        *core;
    struct pw_registry    *registry;

    uint32_t               target_node_id;      // if set, we use this directly
    const char            *match_substr;        // substring to look for in device.description
    uint32_t               found_node_id;       // node we decided to connect to
    int                    connected;

    struct pw_stream      *stream;
};

static volatile int g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static void on_stream_process(void *userdata)
{
    struct app *a = (struct app*)userdata;
    struct pw_buffer *b;

    while ((b = pw_stream_dequeue_buffer(a->stream)) != NULL) {
        struct spa_buffer *sb = b->buffer;
        if (sb->n_datas > 0 && sb->datas[0].data != NULL) {
            const void *data = sb->datas[0].data;
            uint32_t size = sb->datas[0].chunk ? sb->datas[0].chunk->size : sb->datas[0].maxsize;
            if (size > 0) {
                fwrite(data, 1, size, stdout); // write raw PCM to stdout
            }
        }
        pw_stream_queue_buffer(a->stream, b);
    }
}

static void on_stream_param_changed(void *userdata, uint32_t id, const struct spa_pod *param)
{
    if (id != SPA_PARAM_Format || param == NULL) return;

    struct spa_audio_info info = {0};
    spa_format_parse(param, &info.media_type, &info.media_subtype);

    if (info.media_type == SPA_MEDIA_TYPE_audio &&
        info.media_subtype == SPA_MEDIA_SUBTYPE_raw) {
        spa_format_audio_raw_parse(param, &info.info.raw);
        fprintf(stderr,
            "Negotiated format: %s, %u Hz, %u ch\n",
            spa_debug_type_find_name(spa_type_audio_format, info.info.raw.format),
            info.info.raw.rate,
            info.info.raw.channels
        );
    }
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_stream_process,
};

static int connect_stream(struct app *a, uint32_t node_id)
{
    a->stream = pw_stream_new(a->core, "iphone-capture", NULL);
    if (!a->stream) {
        fprintf(stderr, "pw_stream_new failed\n");
        return -1;
    }

    pw_stream_add_listener(a->stream, &((struct pw_stream_listener){0}).link, &stream_events, a);

    // Ask for a common raw format (fallbacks handled by PW):
    struct spa_pod_builder b = {0};
    uint8_t buffer[1024];
    spa_pod_builder_init(&b, buffer, sizeof(buffer));

    struct spa_pod *params[1];
    struct spa_pod *fmt =
        spa_pod_builder_object(&b,
            SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
            SPA_FORMAT_mediaType,        SPA_POD_Id(SPA_MEDIA_TYPE_audio),
            SPA_FORMAT_mediaSubtype,     SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
            SPA_FORMAT_AUDIO_format,     SPA_POD_CHOICE_ENUM_Id(3,
                                        SPA_AUDIO_FORMAT_F32,
                                        SPA_AUDIO_FORMAT_F32,
                                        SPA_AUDIO_FORMAT_S16),
            SPA_FORMAT_AUDIO_rate,       SPA_POD_CHOICE_ENUM_Int(3,
                                        48000, 48000, 44100),
            SPA_FORMAT_AUDIO_channels,   SPA_POD_CHOICE_ENUM_Int(3,
                                        2, 2, 1),
            SPA_FORMAT_AUDIO_position,   SPA_POD_Array(sizeof(uint32_t), SPA_TYPE_Id,
                                        2, (uint32_t[]){ SPA_AUDIO_CHANNEL_FL, SPA_AUDIO_CHANNEL_FR })
        );
    params[0] = fmt;

    int res = pw_stream_connect(a->stream,
                                PW_DIRECTION_INPUT,           // capture
                                node_id,                      // specific node
                                PW_STREAM_FLAG_AUTOCONNECT |
                                PW_STREAM_FLAG_MAP_BUFFERS |
                                PW_STREAM_FLAG_RT_PROCESS,
                                params, 1);
    if (res < 0) {
        fprintf(stderr, "pw_stream_connect failed: %s\n", spa_strerror(res));
        return -1;
    }
    a->connected = 1;
    fprintf(stderr, "Connected to node %u\n", node_id);
    return 0;
}

static void on_global(void *userdata, uint32_t id, uint32_t permissions,
                      const char *type, uint32_t version, const struct spa_dict *props)
{
    struct app *a = (struct app*)userdata;
    (void)permissions;
    (void)version;

    if (a->connected) return; // already connected

    if (strcmp(type, PW_TYPE_INTERFACE_Node) != 0)
        return;

    const char *media_class = props ? spa_dict_lookup(props, "media.class") : NULL;
    if (!media_class || strcmp(media_class, "Audio/Source") != 0)
        return;

    // If explicit node-id was requested, only match that
    if (a->target_node_id != SPA_ID_INVALID) {
        if (id == a->target_node_id && !a->connected) {
            connect_stream(a, id);
        }
        return;
    }

    // Otherwise, match by substring in device.description (default: "iPhone")
    const char *desc = props ? spa_dict_lookup(props, "device.description") : NULL;
    if (!desc) desc = props ? spa_dict_lookup(props, "node.description") : NULL;

    if (desc && strstr(desc, a->match_substr ? a->match_substr : "iPhone")) {
        a->found_node_id = id;
        if (!a->connected) connect_stream(a, id);
    }
}

static const struct pw_registry_events reg_events = {
    PW_VERSION_REGISTRY_EVENTS,
    .global = on_global,
};

static void usage(const char *argv0)
{
    fprintf(stderr,
        "Usage: %s [--match SUBSTR] [--node-id ID]\n"
        "  Reads audio from a PipeWire Audio/Source node and writes raw PCM to stdout.\n"
        "  Default match substring: \"iPhone\".\n\n"
        "Examples:\n"
        "  %s                # first iPhone source\n"
        "  %s --match iPhone # explicit substring\n"
        "  %s --node-id 123  # use a specific Node ID\n",
        argv0, argv0, argv0, argv0
    );
}

int main(int argc, char **argv)
{
    struct app a = {0};
    a.target_node_id = SPA_ID_INVALID;
    a.match_substr = "iPhone";

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--match") && i + 1 < argc) {
            a.match_substr = argv[++i];
        } else if (!strcmp(argv[i], "--node-id") && i + 1 < argc) {
            a.target_node_id = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    // Unbuffer stdout so piping raw audio works immediately
    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    pw_init(&argc, &argv);
    a.loop = pw_main_loop_new(NULL);
    if (!a.loop) { fprintf(stderr, "pw_main_loop_new failed\n"); return 1; }

    struct pw_loop *loop = pw_main_loop_get_loop(a.loop);
    a.context = pw_context_new(loop, NULL, 0);
    if (!a.context) { fprintf(stderr, "pw_context_new failed\n"); return 1; }

    a.core = pw_context_connect(a.context, NULL, 0);
    if (!a.core) { fprintf(stderr, "pw_context_connect failed\n"); return 1; }

    a.registry = pw_core_get_registry(a.core, PW_VERSION_REGISTRY, 0);
    pw_registry_add_listener(a.registry, &((struct pw_registry_listener){0}).link, &reg_events, &a);

    fprintf(stderr, "Waiting for matching Audio/Source node (match=\"%s\"%s)...\n",
            a.match_substr,
            (a.target_node_id != SPA_ID_INVALID ? ", node-id provided" : ""));

    while (!g_stop) {
        pw_main_loop_run(a.loop);
        // pw_main_loop_run blocks; we’ll exit via SIGINT or when process ends
        break;
    }

    if (!a.connected) {
        fprintf(stderr, "No matching node found. Is the iPhone streaming/connected?\n");
    }

    if (a.stream) pw_stream_destroy(a.stream);
    if (a.registry) pw_proxy_destroy((struct pw_proxy*)a.registry);
    if (a.core) pw_core_disconnect(a.core);
    if (a.context) pw_context_destroy(a.context);
    if (a.loop) pw_main_loop_destroy(a.loop);
    pw_deinit();
    return a.connected ? 0 : 2;
}