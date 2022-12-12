/* audiocap-internal.h
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
typedef struct audio_thread_pa_i * audio_thread_pa;

audio_thread_pa create_audio_thread_pa(bool microphone, asr_thread asr);
void *run_audio_thread_pa(void *thread);
void free_audio_thread_pa(audio_thread_pa thread);


#ifdef LIVE_CAPTIONS_PIPEWIRE
struct audio_thread_pw_i;
typedef struct audio_thread_pw_i * audio_thread_pw;

audio_thread_pw create_audio_thread_pw(bool microphone, asr_thread asr);
void *run_audio_thread_pw(void *thread);
void free_audio_thread_pw(audio_thread_pw thread);
#endif
