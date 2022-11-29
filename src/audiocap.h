#pragma once

#include <adwaita.h>

struct audio_thread_i;
typedef struct audio_thread_i * audio_thread;


audio_thread create_audio_thread();
void audio_thread_set_label(audio_thread thread, GtkLabel *label);
void free_audio_thread(audio_thread thread);