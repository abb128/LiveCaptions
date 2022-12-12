/* audiocap.h
 * This file contains declarations for audio_thread, which reads system audio
 * and passes them to the provided asr_thread.
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

#include "asrproc.h"

struct audio_thread_i;
typedef struct audio_thread_i * audio_thread;

audio_thread create_audio_thread(bool microphone, asr_thread asr);
void free_audio_thread(audio_thread thread);
