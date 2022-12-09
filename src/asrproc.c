#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <glib.h>

#include <stdbool.h>
#include <april_api.h>

#include "asrproc.h"
#include "line-gen.h"

struct asr_thread_i {
    volatile size_t sound_counter;
    size_t silence_counter;

    GThread * thread_id;

    struct line_generator line;

    GMutex text_mutex;
    char text_buffer[32768];

    AprilASRModel model;
    AprilASRSession session;

    GtkLabel *label;

    volatile bool pause;
};


void *run_asr_thread(void *userdata) {
    asr_thread data = (asr_thread)userdata;

    //sleep(40);

    //if(data->sound_counter < 512) {
    //    gtk_label_set_text(data->label, "[No audio is being received. If you are playing audio,\nmost likely the audio recording isn't working]");
    //}

    return NULL;
}

gboolean main_thread_update_label(void *userdata){
    asr_thread data = userdata;

    if((data->label == NULL) || (data->pause)) return G_SOURCE_REMOVE;

    // trylock throws attempt to unlock mutex that's not locked sometimes for osme reason??
    g_mutex_lock(&data->text_mutex);

    line_generator_set_text(&data->line, data->label);

    g_mutex_unlock(&data->text_mutex);


    return G_SOURCE_REMOVE;
}

void april_result_handler(void* userdata, AprilResultType result, size_t count, const AprilToken* tokens) {
    asr_thread data = userdata;

    g_mutex_lock(&data->text_mutex);

    line_generator_update(&data->line, count, tokens);
    if(result == APRIL_RESULT_RECOGNITION_FINAL) {
        line_generator_finalize(&data->line);
    }

    g_mutex_unlock(&data->text_mutex);
    g_idle_add(main_thread_update_label, data);
}

void asr_thread_enqueue_audio(asr_thread thread, short *data, size_t num_shorts) {
    if((thread->label == NULL) || thread->pause) return;

    bool found_nonzero = false;
    for(size_t i=0; i<num_shorts; i++){
        if((data[i] > 16) || (data[i] < -16)){
            found_nonzero = true;
            break;
        }
    }

    thread->silence_counter = found_nonzero ? 0 : (thread->silence_counter + num_shorts);

    if(thread->silence_counter >= 24000){
        thread->silence_counter = 24000;
        return aas_flush(thread->session);
    }
    
    thread->sound_counter += num_shorts;
    aas_feed_pcm16(thread->session, data, num_shorts); // TODO?
}

gpointer asr_thread_get_model(asr_thread thread) {
    return thread->model;
}

void asr_thread_pause(asr_thread thread, bool pause) {
    thread->pause = pause;
}

int asr_thread_samplerate(asr_thread thread) {
    return aam_get_sample_rate(thread->model);
}

asr_thread create_asr_thread(const char *model_path){
    asr_thread data = calloc(1, sizeof(struct asr_thread_i));

    line_generator_init(&data->line);

    data->model = aam_create_model(model_path);
    if(data->model == NULL) {
        printf("Failed to create model\n");
        return NULL;
    }

    AprilConfig config = {
        .handler = april_result_handler,
        .userdata = data
    };

    data->session = aas_create_session(data->model, config);

    if(data->session == NULL){
        printf("Failed to create session\n");
        return NULL;
    }

    g_mutex_init(&data->text_mutex);

    data->thread_id = g_thread_new("lcap-audiothread", run_asr_thread, data);

    return data;
}

void asr_thread_set_label(asr_thread thread, GtkLabel *label) {
    thread->label = label;
}

void asr_thread_flush(asr_thread thread) {
    aas_flush(thread->session);
}

void free_asr_thread(asr_thread thread) {
    g_thread_join(thread->thread_id);

    aas_free(thread->session);
    aam_free(thread->model);

    g_thread_unref(thread->thread_id); // ?

    free(thread);
}
