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

#define USE_MARKUP

#define AC_LINE_MAX 4096
#define AC_LINE_COUNT 2
#define REL_LINE_IDX(HEAD, IDX) (4*AC_LINE_COUNT + (HEAD) + (IDX)) % AC_LINE_COUNT

struct line {
    char text[AC_LINE_MAX];
    size_t head;
    size_t len;
};

struct line_generator {
    size_t current_line;
    struct line lines[AC_LINE_COUNT];

    // Denotes the index within the active token array at which the line starts
    // If -1, means the active tokens don't reach that line yet
    ssize_t active_start_of_lines[AC_LINE_COUNT];

    char output[AC_LINE_MAX * AC_LINE_COUNT];
};

void line_generator_init(struct line_generator *lg) {
    for(int i=0; i<AC_LINE_COUNT; i++){
        lg->active_start_of_lines[i] = -1;
    }

    lg->current_line = 0;
    lg->active_start_of_lines[0] = 0;
}

void line_generator_update(struct line_generator *lg, size_t num_tokens, const AprilToken *tokens) {
    for(int i=0; i<AC_LINE_COUNT; i++){
        if(lg->active_start_of_lines[i] == -1) continue;

        struct line *curr = &lg->lines[i];

        // reset for writing
        curr->text[0] = '\0';
        curr->head = 0;
        curr->len = 0;

        if(lg->active_start_of_lines[i] >= num_tokens) {
            printf("%d more tokens than exist %d!\n", lg->active_start_of_lines[i], num_tokens);
            if(i == lg->current_line) {
                // oops... turns out our text isn't long enough for the new line
                // backtrack to the previous line
                lg->active_start_of_lines[lg->current_line] = -1;
                lg->current_line = REL_LINE_IDX(lg->current_line, -1);
                return line_generator_update(lg, num_tokens, tokens); // TODO?
            } else {
                continue;
            }
        }


        int end = lg->active_start_of_lines[REL_LINE_IDX(i, 1)];
        if((end == -1) || (i == lg->current_line)) end = num_tokens;

        // print line
        for(int j=lg->active_start_of_lines[i]; j<end; j++) {
            bool can_break_nicely = (curr->len > 48) && (tokens[j].token[0] == ' ') && (tokens[j].logprob > -1.0f);
            bool must_break = (curr->head > (AC_LINE_MAX - 256));
            if((i == lg->current_line) && (can_break_nicely  || must_break)) {
                // line break
                lg->current_line = REL_LINE_IDX(lg->current_line, 1);
                lg->active_start_of_lines[lg->current_line] = j;
                return line_generator_update(lg, num_tokens, tokens); // TODO?
            }

            if(must_break){
                printf("Must linebreak, but not active line. Leaving incomplete line...\n");
                break;
            }

            int alpha = (int)((tokens[j].logprob + 2.0) / 8.0 * 65536.0);
            alpha /= 2.0;
            alpha += 32768;
            if(alpha < 10000) alpha = 10000;
            if(alpha > 65535) alpha = 65535;
            curr->head += sprintf(&curr->text[curr->head], "<span fgalpha=\"%d\">%s</span>", alpha, tokens[j].token);
            g_assert(curr->head < AC_LINE_MAX);

            curr->len += strlen(tokens[j].token);
        }
    }
}

void line_generator_finalize(struct line_generator *lg) {
    // fix when new line contains only like 1 token
    // ......

    // insert new line
    lg->current_line = REL_LINE_IDX(lg->current_line, 1);

    // reset active
    for(int i=0; i<AC_LINE_COUNT; i++) lg->active_start_of_lines[i] = -1;

    // set new line to start at 0
    lg->active_start_of_lines[lg->current_line] = 0;

    // clear new line
    lg->lines[lg->current_line].text[0] = '\0';
    lg->lines[lg->current_line].head = 0;
}

void line_generator_set_text(struct line_generator *lg, GtkLabel *lbl) {
    char *head = &lg->output[0];
    *head = '\0';

    for(int i=AC_LINE_COUNT-1; i>=0; i--) {
        struct line *curr = &lg->lines[REL_LINE_IDX(lg->current_line, -i)];
        head += sprintf(head, "%s", curr->text);

        if(i != 0) head += sprintf(head, "\n");
    }

    for(int i=0; i<(AC_LINE_MAX * AC_LINE_COUNT); i++){
        if(lg->output[i] == '\0') break;
        lg->output[i] = tolower(lg->output[i]);
    }

    gtk_label_set_markup(lbl, lg->output);
}

struct audio_thread_i {
    GThread * thread_id;

    struct line_generator line;

    GMutex text_mutex;
    char text_buffer[32768];

    AprilASRModel model;
    AprilASRSession session;

    GtkLabel *label;

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
static void on_process(void *userdata)
{
    audio_thread data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    short *samples;
    uint32_t n_channels, n_samples;

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL)
        return;

    n_channels = data->format.info.raw.channels;
    n_samples = buf->datas[0].chunk->size / sizeof(short);

    g_assert(n_channels == 1);

    aas_feed_pcm16(data->session, samples, n_samples);

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

gboolean main_thread_update_label(void *userdata){
    audio_thread data = userdata;

    // trylock throws attempt to unlock mutex that's not locked sometimes for osme reason??
    g_mutex_lock(&data->text_mutex);

    line_generator_set_text(&data->line, data->label);

    g_mutex_unlock(&data->text_mutex);


    return G_SOURCE_REMOVE;
}

void april_result_handler(void* userdata, AprilResultType result, size_t count, const AprilToken* tokens) {
    audio_thread data = userdata;

    g_mutex_lock(&data->text_mutex);


    line_generator_update(&data->line, count, tokens);
    if(result == APRIL_RESULT_RECOGNITION_FINAL) {
        line_generator_finalize(&data->line);
    }

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

    /*
    int line_width = 0;
    for(int t=0; t<count; t++){
        const char *text = tokens[t].token;
        if((line_width > 48) && (text[0] == ' ')){
            txt_head += sprintf(txt_head, "\n");
            line_width = 0;
        }

#ifdef USE_MARKUP
        int alpha = (int)((tokens[t].logprob + 2.0) / 8.0 * 65536.0);
        alpha /= 2.0;
        alpha += 32768;
        if(alpha < 10000) alpha = 10000;
        if(alpha > 65535) alpha = 65535;
        int l = sprintf(txt_head, "<span fgalpha=\"%d\">%s</span>", alpha, text);
#else
        int l = sprintf(txt_head, "%s", text);
#endif
        line_width += strlen(text);
        txt_head += l;

        //printf("%s", text);
    }
    //printf("\n");

    */
    g_mutex_unlock(&data->text_mutex);
    g_idle_add(main_thread_update_label, data);
}

audio_thread create_audio_thread(){
    audio_thread data = calloc(1, sizeof(struct audio_thread_i));

    line_generator_init(&data->line);

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

    g_mutex_init(&data->text_mutex);

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