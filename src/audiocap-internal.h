#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <glib.h>

#include "asrproc.h"
#include "audiocap.h"

struct audio_thread_pa_i;
struct audio_thread_pw_i;
typedef struct audio_thread_pa_i * audio_thread_pa;
typedef struct audio_thread_pw_i * audio_thread_pw;



audio_thread_pa create_audio_thread_pa(bool microphone, asr_thread asr);
void *run_audio_thread_pa(void *thread);
void free_audio_thread_pa(audio_thread_pa thread);


audio_thread_pw create_audio_thread_pw(bool microphone, asr_thread asr);
void *run_audio_thread_pw(void *thread);
void free_audio_thread_pw(audio_thread_pw thread);