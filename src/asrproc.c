/* asrproc.c
 * This file implements asr_thread which takes in audio, passes it to aprilasr,
 * and passes the output to line_generator to update the GtkLabel.
 *
 * Copyright 2022 abb128
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "livecaptions-window.h"
#include "history.h"

struct asr_thread_i {
    volatile size_t sound_counter;
    size_t silence_counter;

    GThread * thread_id;

    struct line_generator line;

    GMutex text_mutex;
    char text_buffer[32768];

    AprilASRModel model;
    AprilASRSession session;

    LiveCaptionsWindow *window;

    size_t layout_counter;

    volatile bool pause;
};


static void *run_asr_thread(void *userdata) {
    asr_thread data = (asr_thread)userdata;

    //sleep(40);

    //if(data->sound_counter < 512) {
    //    gtk_label_set_text(data->label, "[No audio is being received. If you are playing audio,\nmost likely the audio recording isn't working]");
    //}

    return NULL;
}

static gboolean main_thread_update_label(void *userdata){
    asr_thread data = userdata;

    if((data->window == NULL) || (data->pause)) return G_SOURCE_REMOVE;

    g_mutex_lock(&data->text_mutex);
    line_generator_set_text(&data->line, data->window->label);
    g_mutex_unlock(&data->text_mutex);

    return G_SOURCE_REMOVE;
}

static void april_result_handler(void* userdata, AprilResultType result, size_t count, const AprilToken* tokens) {
    asr_thread data = userdata;
    if((data->window == NULL) || (data->pause)) return;

    switch(result) {
        case APRIL_RESULT_RECOGNITION_PARTIAL:
        case APRIL_RESULT_RECOGNITION_FINAL:
        {
            g_mutex_lock(&data->text_mutex);

            if((data->layout_counter != data->window->font_layout_counter) || (data->line.layout == NULL)) {
                if(data->line.layout != NULL) g_object_unref(data->line.layout);

                data->line.layout = pango_layout_copy(data->window->font_layout);
                data->line.max_text_width = data->window->max_text_width;

                data->layout_counter = data->window->font_layout_counter;
            }

            line_generator_update(&data->line, count, tokens);
            if(result == APRIL_RESULT_RECOGNITION_FINAL) {
                line_generator_finalize(&data->line);
                commit_tokens_to_current_history(tokens, count);
            }

            g_mutex_unlock(&data->text_mutex);
            g_idle_add(main_thread_update_label, data);
            break;
        }

        case APRIL_RESULT_ERROR_CANT_KEEP_UP: {
            livecaptions_window_warn_slow(data->window);
            break;
        }

        case APRIL_RESULT_SILENCE: {
            g_mutex_lock(&data->text_mutex);

            line_generator_break(&data->line);
            save_silence_to_history();

            g_mutex_unlock(&data->text_mutex);
            g_idle_add(main_thread_update_label, data);
            break;
        }
    }
}

void asr_thread_enqueue_audio(asr_thread thread, short *data, size_t num_shorts) {
    if((thread->window == NULL) || thread->pause) return;

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

gpointer asr_thread_get_session(asr_thread thread) {
    return thread->session;
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
        .flags = APRIL_CONFIG_FLAG_REALTIME_BIT,
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

void asr_thread_set_main_window(asr_thread thread, LiveCaptionsWindow *window) {
    thread->window = window;
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
