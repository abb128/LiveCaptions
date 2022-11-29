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

#include "audiocap.h"

#define LINE_MAX 256
#define LINE_COUNT 2


// ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
/*struct line_generator {
    struct {
        char text[LINE_MAX];
        size_t head;
    } lines[LINE_COUNT];

    size_t head;
};

void line_generator_linebreak(struct line_generator *lg) {
    lg->head = (lg->head + 1) % LINE_COUNT;

    lg->lines[lg->head].head = 0;
    lg->lines[lg->head].text[0] = '\0';
}

void line_generator_push_token(struct line_generator *lg, AprilToken token){
    if((token.token[0] == ' ') && (lg->lines[lg->head].head > 48)) {
        line_generator_linebreak(lg);
    }

    lg->lines[lg->head].head += sprintf(&lg->lines[lg->head].text[lg->lines[lg->head].head], "%s", tokens.token);
}
*/

struct audio_thread_i {
    GThread * thread_id;

    //struct line_generator line;

    AprilASRModel model;
    AprilASRSession session;

    GtkLabel *label;

    size_t sample_rate;
    size_t channels;

    struct pw_main_loop *loop;
    struct pw_stream *stream;

    struct spa_audio_info format;
    unsigned move:1;
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
static void on_process(void *userdata)
{
    audio_thread data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    short *samples, max;
    uint32_t c, n, n_channels, n_samples, peak;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL)
        return;

    n_channels = data->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(short);

    aas_feed_pcm16(data->session, samples, n_samples);
    ///* move cursor up */
    //if (data->move)
    //    fprintf(stdout, "%c[%dA", 0x1b, n_channels + 1);
    //fprintf(stdout, "captured %d samples\n", n_samples / n_channels);
    //for (c = 0; c < data->format.info.raw.channels; c++) {
    //    max = 0;
    //    for (n = c; n < n_samples; n += n_channels) {
    //        if((samples[n]) > max) max = (samples[n]);
    //    }
//
    //    peak = SPA_CLAMP(((float)max / 16384.0f) * 30, 0, 39);
//
    //    fprintf(stdout, "channel %d: |%*s%*s| peak:%f\n",
    //            c, peak+1, "*", 40 - peak, "", (float)max / 16384.0f);
    //}
    //data->move = true;
    //fflush(stdout);

    pw_stream_queue_buffer(data->stream, b);
}

/* Be notified when the stream param changes. We're only looking at the
 * format changes.
 */
static void
on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param)
{
    audio_thread data = _data;

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

static void do_quit(void *userdata, int signal_number) {
    audio_thread data = userdata;
    pw_main_loop_quit(data->loop);
}

void *run_audio_thread(void *userdata) {
    audio_thread data = (audio_thread)userdata;

    const struct spa_pod *params[1];
    uint8_t buffer[1024];
    struct pw_properties *props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));

    data->loop = pw_main_loop_new(NULL);

    pw_loop_add_signal(pw_main_loop_get_loop(data->loop), SIGINT, do_quit, data);
    pw_loop_add_signal(pw_main_loop_get_loop(data->loop), SIGTERM, do_quit, data);

    /* Create a simple stream, the simple stream manages the core and remote
     * objects for you if you don't need to deal with them.
     *
     * If you plan to autoconnect your stream, you need to provide at least
     * media, category and role properties.
     *
     * Pass your events and a user_data pointer as the last arguments. This
     * will inform you about the stream state. The most important event
     * you need to listen to is the process event where you need to produce
     * the data->
     */
    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Monitor",
            PW_KEY_MEDIA_ROLE, "Accessibility",
            PW_KEY_STREAM_CAPTURE_SINK, "true",
            PW_KEY_NODE_NAME, "LiveCaptions",
            //PW_KEY_NODE_LATENCY, "1/1000",
            NULL);

    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");

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
                .rate = aam_get_sample_rate(data->model),
                .channels = 1 ));

    /* Now connect this stream. We ask that our process function is
     * called in a realtime thread. */
    pw_stream_connect(data->stream,
              PW_DIRECTION_INPUT,
              PW_ID_ANY,
              PW_STREAM_FLAG_AUTOCONNECT |
              PW_STREAM_FLAG_MAP_BUFFERS |
              PW_STREAM_FLAG_RT_PROCESS,
              params, 1);

    /* and wait while we let things run */
    pw_main_loop_run(data->loop);

    pw_stream_destroy(data->stream);
    pw_main_loop_destroy(data->loop);
    
    return NULL;
}

void april_result_handler(void* userdata, AprilResultType result, size_t count, const AprilToken* tokens) {
    char text[1024];

    char *txt_head = text;
    //switch(result){
    //    case APRIL_RESULT_RECOGNITION_FINAL: 
    //        txt_head += sprintf(txt_head, "@ ");
    //        break;
    //    case APRIL_RESULT_RECOGNITION_PARTIAL:
    //        txt_head += sprintf(txt_head, "- ");
    //        break;
    //    default:
    //        txt_head += sprintf(txt_head, "? ");
    //        break;
    //}

    int line_width = 0;
    for(int t=0; t<count; t++){
        const char *text = tokens[t].token;
        if((line_width > 48) && (text[0] == ' ')){
            txt_head += sprintf(txt_head, "\n");
            line_width = 0;
        }
        int l = sprintf(txt_head, "%s", text);
        line_width += l;
        txt_head += l;

        //printf("%s", text);
    }
    //printf("\n");

    audio_thread data = userdata;

    for(int i=0; i<1024; i++){
        if(text[i] == '\0') break;
        text[i] = tolower(text[i]);
    }

    gtk_label_set_text(data->label, text);
}

audio_thread create_audio_thread(){
    audio_thread data = calloc(1, sizeof(struct audio_thread_i));

    data->model = aam_create_model("/run/media/alex/EncSSD/ASR/own-py/aprilv0_en-us.april");
    if(data->model == NULL) {
        printf("Failed to create model\n");
        return NULL;
    }

    data->session = aas_create_session(
        data->model,
        april_result_handler,
        data,
        NULL
    );

    if(data->session == NULL){
        printf("Failed to create session\n");
        return NULL;
    }



    data->thread_id = g_thread_new("lcap-audiothread", run_audio_thread, data);

    return data;
}

void audio_thread_set_label(audio_thread thread, GtkLabel *label) {
    thread->label = label;
}

void free_audio_thread(audio_thread thread) {
    aas_free(thread->session);
    aam_free(thread->model);

    /// TODO
    //pthread_kill(thread->thread_id, SIGINT);
    //pthread_join(thread->thread_id, NULL);

    free(thread);
}