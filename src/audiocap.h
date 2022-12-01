#pragma once

#include <adwaita.h>
#include "asrproc.h"

struct audio_thread_i;
typedef struct audio_thread_i * audio_thread;


audio_thread create_audio_thread(bool microphone, asr_thread asr);
void free_audio_thread(audio_thread thread);