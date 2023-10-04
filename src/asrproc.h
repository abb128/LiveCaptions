/* asrproc.h
 * This file contains declarations for asr_thread which performs the ASR
 * processing.
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

#include <adwaita.h>

struct _LiveCaptionsWindow;

struct asr_thread_i;
typedef struct asr_thread_i * asr_thread;


asr_thread create_asr_thread(const char *model_path);
bool asr_thread_update_model(asr_thread thread, const char *model_path);
bool asr_thread_is_errored(asr_thread thread);
void asr_thread_set_main_window(asr_thread thread, struct _LiveCaptionsWindow *window);
void asr_thread_enqueue_audio(asr_thread thread, short *data, size_t num_shorts);
gpointer asr_thread_get_model(asr_thread thread);
gpointer asr_thread_get_session(asr_thread thread);
void asr_thread_pause(asr_thread thread, bool pause);
void asr_thread_set_text_stream_active(asr_thread thread, bool active);
int asr_thread_samplerate(asr_thread thread);
void asr_thread_flush(asr_thread thread);
void free_asr_thread(asr_thread thread);
