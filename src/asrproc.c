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
    size_t silence_counter;

    FILE *fd;

    GThread * thread_id;

    struct line_generator line;

    GMutex text_mutex;
    char text_buffer[32768];

    AprilASRModel model;
    AprilASRSession session;

    GtkLabel *label;
};


void *run_asr_thread(void *userdata) {
    asr_thread data = (asr_thread)userdata;

    return NULL;
}

gboolean main_thread_update_label(void *userdata){
    asr_thread data = userdata;

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
    bool found_nonzero = false;
    for(int i=0; i<num_shorts; i++){
        if((data[i] > 8) || (data[i] < -8)){
            found_nonzero = true;
            break;
        }
    }

    thread->silence_counter = found_nonzero ? 0 : (thread->silence_counter + num_shorts);

    if(thread->silence_counter >= 24000){
        thread->silence_counter = 24000;
        return aas_flush(thread->session);
    }
    
    fwrite(data, num_shorts, 2, thread->fd);
    aas_feed_pcm16(thread->session, data, num_shorts); // TODO?
}

asr_thread create_asr_thread(){
    asr_thread data = calloc(1, sizeof(struct asr_thread_i));

    data->fd = fopen("/tmp/debug.bin", "w");
    line_generator_init(&data->line);

    data->model = aam_create_model("/run/media/alex/EncSSD/ASR/own-py/aprilv0_en-us.april");
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

void free_asr_thread(asr_thread thread) {
    g_thread_join(thread->thread_id);

    aas_free(thread->session);
    aam_free(thread->model);

    fclose(thread->fd);

    g_thread_unref(thread->thread_id); // ?

    free(thread);
}