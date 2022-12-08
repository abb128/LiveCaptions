#include "audiocap.h"
#include "audiocap-internal.h"

#include <pulse/pulseaudio.h>
#include <april_api.h>

#define USE_PULSEAUDIO 1

struct audio_thread_i {
    GThread * thread_id;

    bool using_pulse;
    union {
        audio_thread_pa pulse;
#ifdef LIVE_CAPTIONS_PIPEWIRE
        audio_thread_pw pipewire;
#endif
    } thread;
};


void *run_audio_thread(void *userdata) {
    audio_thread data = (audio_thread)userdata;

    return NULL;
}

audio_thread create_audio_thread(bool microphone, asr_thread asr){
    audio_thread data = calloc(1, sizeof(struct audio_thread_i));
    
    data->using_pulse = USE_PULSEAUDIO;

    if(data->using_pulse) {
        data->thread.pulse = create_audio_thread_pa(microphone, asr);
        data->thread_id = g_thread_new("lcap-audiothread", run_audio_thread_pa, data->thread.pulse);
    }
#ifdef LIVE_CAPTIONS_PIPEWIRE
      else {
        data->thread.pipewire = create_audio_thread_pw(microphone, asr);
        data->thread_id = g_thread_new("lcap-audiothread", run_audio_thread_pw, data->thread.pipewire);
    }
#endif

    return data;
}

void free_audio_thread(audio_thread thread) {
    if(thread->using_pulse){
        free_audio_thread_pa(thread->thread.pulse);
    }
#ifdef LIVE_CAPTIONS_PIPEWIRE
      else {
        free_audio_thread_pw(thread->thread.pipewire);
    }
#endif

    g_thread_join(thread->thread_id);

    g_thread_unref(thread->thread_id); // ?


    if(thread->using_pulse){
        free(thread->thread.pulse);
    }
#ifdef LIVE_CAPTIONS_PIPEWIRE
      else {
        free(thread->thread.pipewire);
    }
#endif

    free(thread);
}
