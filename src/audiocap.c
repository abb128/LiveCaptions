/* audiocap.c
 * This file implements audio_thread using either the pipewire or pulse backend.
 * Currently only the pulse backend is used.
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

#include "audiocap.h"
#include "audiocap-internal.h"

#include <pulse/pulseaudio.h>
#include <april_api.h>

#define USE_PULSEAUDIO 1

struct audio_thread_i {
#ifdef LIVE_CAPTIONS_PIPEWIRE
    GThread * pw_thread_id;
#endif

    bool using_pulse;
    union {
        audio_thread_pa pulse;
#ifdef LIVE_CAPTIONS_PIPEWIRE
        audio_thread_pw pipewire;
#endif
    } thread;
};


static void *run_audio_thread(void *userdata) {
    audio_thread data = (audio_thread)userdata;

    return NULL;
}

audio_thread create_audio_thread(bool microphone, asr_thread asr){
    audio_thread data = calloc(1, sizeof(struct audio_thread_i));
    
    data->using_pulse = USE_PULSEAUDIO;

    if(data->using_pulse) {
        data->thread.pulse = create_audio_thread_pa(microphone, asr);
        run_audio_thread_pa(data->thread.pulse);
    }
#ifdef LIVE_CAPTIONS_PIPEWIRE
      else {
        data->thread.pipewire = create_audio_thread_pw(microphone, asr);
        data->pw_thread_id = g_thread_new("lcap-audiothread", run_audio_thread_pw, data->thread.pipewire);
    }
#endif

    return data;
}

void free_audio_thread(audio_thread thread) {
    if(thread->using_pulse){
        free_audio_thread_pa(thread->thread.pulse);
        free(thread->thread.pulse);
    }
#ifdef LIVE_CAPTIONS_PIPEWIRE
      else {
        free_audio_thread_pw(thread->thread.pipewire);
        g_thread_join(thread->pw_thread_id);
        g_thread_unref(thread->pw_thread_id); // ?
        free(thread->thread.pipewire);
    }
#endif

    free(thread);
}
