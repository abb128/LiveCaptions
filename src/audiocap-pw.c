#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <spa/param/audio/format-utils.h>
#include <pipewire/pipewire.h>
#include <glib.h>

#include <april_api.h>

#include "audiocap-internal.h"
#include "audiocap.h"

struct audio_thread_pw_i {
    asr_thread asr;

    bool microphone;
    size_t sample_rate;
    size_t channels;

    struct pw_main_loop *loop;
    struct pw_stream *stream;

    struct spa_audio_info format;
};

/* our data processing function is in general:
 *
 *  struct pw_buffer *b;
 *  b = pw_stream_dequeue_buffer(stream);
 *
 *  .. consume stuff in the buffer ...
 *
 *  pw_stream_queue_buffer(stream, b);
 */
static void on_process(void *userdata) {
    audio_thread_pw data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    short *samples;
    uint32_t n_channels, n_samples;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL) {
        printf("null buff\n");
        return;
    }

    n_channels = data->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(short);

    g_assert(n_channels == 1);
    g_assert(sizeof(short) == 2);

    if(data->asr != NULL){
        asr_thread_enqueue_audio(data->asr, samples, n_samples);
    }
    // ...

    pw_stream_queue_buffer(data->stream, b);
}


static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param) {
    audio_thread_pw data = _data;

    /* NULL means to clear the format */
    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param, &data->format.media_type, &data->format.media_subtype) < 0)
        return;

    /* only accept raw audio */
    if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    /* call a helper function to parse the format for us. */
    spa_format_audio_raw_parse(param, &data->format.info.raw);

    fprintf(stdout, "capturing rate:%d channels:%d\n",
            data->format.info.raw.rate, data->format.info.raw.channels);

    data->sample_rate = data->format.info.raw.rate;
    data->channels = data->format.info.raw.channels;
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *userdata, G_GNUC_UNUSED int signal_number) {
    audio_thread_pw data = userdata;
    pw_main_loop_quit(data->loop);
}

void *run_audio_thread_pw(void *userdata) {
    audio_thread_pw data = (audio_thread_pw)userdata;

    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct pw_properties *props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    data->loop = pw_main_loop_new(NULL);

    pw_loop_add_signal(pw_main_loop_get_loop(data->loop), SIGINT, do_quit, data);
    pw_loop_add_signal(pw_main_loop_get_loop(data->loop), SIGTERM, do_quit, data);

    unsigned int latency_ms = 100;
    unsigned int rate = data->sample_rate;
    unsigned int nom = latency_ms * rate / 1000;

    props = pw_properties_new(
            PW_KEY_MEDIA_TYPE,     "Audio",
            PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_NODE_NAME,      "LiveCaptions",
            NULL);
    
    pw_properties_setf(props, PW_KEY_NODE_RATE, "1/%u", rate);
    pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%u", nom, rate);
    pw_properties_set(props, PW_KEY_AUDIO_FORMAT, "S16");

    if(data->microphone) {
        // Ask pipewire to capture microphone input
        pw_properties_set(props, PW_KEY_MEDIA_ROLE, "Communication");
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "false");
    } else {
        // Ask pipewire to capture desktop audio
        pw_properties_set(props, PW_KEY_MEDIA_ROLE, "Accessibility");
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    }

    data->stream = pw_stream_new_simple(
            pw_main_loop_get_loop(data->loop),
            "audio-capture",
            props,
            &stream_events,
            data);

    /* Make one parameter with the supported formats. The SPA_PARAM_EnumFormat
     * id means that this is a format enumeration (of 1 value).
     * We leave the channels and rate empty to accept the native graph
     * rate and channels. */
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat,
            &SPA_AUDIO_INFO_RAW_INIT(
                .format = SPA_AUDIO_FORMAT_S16,
                .rate = rate,
                .channels = 1 ));

    /* Now connect this stream. We ask that our process function is
     * called in a realtime thread. */
    pw_stream_connect(data->stream,
              PW_DIRECTION_INPUT,
              PW_ID_ANY,
              PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS,
                      // TODO: RT_PROCESS may not be correct here, but omitting
                      // it causes on_process to stop getting called sometimes
                      // for some reason
              params, 1);

    /* and wait while we let things run */
    pw_main_loop_run(data->loop);

    pw_stream_destroy(data->stream);
    pw_main_loop_destroy(data->loop);
    
    return NULL;
}

audio_thread_pw create_audio_thread_pw(bool microphone, asr_thread asr){
    audio_thread_pw data = calloc(1, sizeof(struct audio_thread_pw_i));
    
    data->microphone = microphone;
    data->asr = asr;
    data->sample_rate = asr_thread_samplerate(asr);

    return data;
}

void free_audio_thread_pw(audio_thread_pw thread) {
    pw_main_loop_quit(thread->loop);
}
