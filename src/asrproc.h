#pragma once

#include <adwaita.h>

struct asr_thread_i;
typedef struct asr_thread_i * asr_thread;


asr_thread create_asr_thread(const char *model_path);
void asr_thread_set_label(asr_thread thread, GtkLabel *label);
void asr_thread_enqueue_audio(asr_thread thread, short *data, size_t num_shorts);
gpointer asr_thread_get_model(asr_thread thread);
void asr_thread_pause(asr_thread thread, bool pause);
int asr_thread_samplerate(asr_thread thread);
void asr_thread_flush(asr_thread thread);
void free_asr_thread(asr_thread thread);