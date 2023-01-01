/* audiocap-pa.c
 * This file contains the pulse implementation of audio_thread
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

#include <pulse/introspect.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <pulse/pulseaudio.h>
#include <glib.h>

#include <april_api.h>

#include "audiocap-internal.h"
#include "audiocap.h"

struct audio_thread_pa_i {
    asr_thread asr;
    bool microphone;
    size_t sample_rate;

    char *sink_name;
    char *source_name;

    pa_threaded_mainloop *mainloop;
    pa_mainloop_api *mainloop_api;
    pa_context *context;
    pa_stream *stream;

};

static void context_state_cb(pa_context* context, void* userdata);
static void stream_state_cb(pa_stream *s, void *userdata);
static void stream_success_cb(pa_stream *stream, int success, void *userdata);
static void stream_read_cb(pa_stream *stream, size_t nbytes, void *userdata);

static void server_info_callback(pa_context *c, const pa_server_info *i, void *userdata){
    audio_thread_pa data = (audio_thread_pa)userdata;

    data->source_name = (char *)calloc(1, strlen(i->default_source_name) + 1);
    strcpy(data->source_name, i->default_source_name);

    data->sink_name = (char *)calloc(1, strlen(i->default_sink_name) + 9);
    strcpy(data->sink_name, i->default_sink_name);
    strcat(data->sink_name, ".monitor");

    pa_threaded_mainloop_signal(data->mainloop, 0);
}

void *run_audio_thread_pa(void *userdata) {
    audio_thread_pa data = (audio_thread_pa)userdata;

    // Get a mainloop and its context
    data->mainloop = pa_threaded_mainloop_new();
    g_assert(data->mainloop);
    data->mainloop_api = pa_threaded_mainloop_get_api(data->mainloop);
    data->context = pa_context_new(data->mainloop_api, "lcap-acap");
    g_assert(data->context);

    // Set a callback so we can wait for the context to be ready
    pa_context_set_state_callback(data->context, &context_state_cb, data);

    // Lock the mainloop so that it does not run and crash before the context is ready
    pa_threaded_mainloop_lock(data->mainloop);

    // Start the mainloop
    g_assert(pa_threaded_mainloop_start(data->mainloop) == 0);
    g_assert(pa_context_connect(data->context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) == 0);

    // Wait for the context to be ready
    for(;;) {
        pa_context_state_t context_state = pa_context_get_state(data->context);
        assert(PA_CONTEXT_IS_GOOD(context_state));
        if (context_state == PA_CONTEXT_READY) break;
        pa_threaded_mainloop_wait(data->mainloop);
    }

    pa_context_get_server_info(data->context, server_info_callback, data);
    for(;;) {
        if(data->source_name != NULL) break;
        pa_threaded_mainloop_wait(data->mainloop);
    }

    // Create a recording stream
    pa_sample_spec sample_specifications;
    sample_specifications.format = PA_SAMPLE_S16LE;
    sample_specifications.rate = data->sample_rate;
    sample_specifications.channels = 1;

    pa_channel_map map;
    pa_channel_map_init_mono(&map);

    data->stream = pa_stream_new(data->context, "Record", &sample_specifications, &map);
    g_assert(data->stream);

    pa_stream_set_state_callback(data->stream, stream_state_cb, data);
    pa_stream_set_read_callback(data->stream, stream_read_cb, data);

    // recommended settings, i.e. server uses sensible values
    pa_buffer_attr buffer_attr;
    buffer_attr.maxlength = (uint32_t) -1;
    buffer_attr.tlength = (uint32_t) -1;
    buffer_attr.prebuf = (uint32_t) -1;
    buffer_attr.minreq = (uint32_t) -1;
    buffer_attr.fragsize = pa_usec_to_bytes(50000, &sample_specifications);

    // Settings copied as per the chromium browser source
    pa_stream_flags_t stream_flags;
    stream_flags = PA_STREAM_START_CORKED | PA_STREAM_INTERPOLATE_TIMING |
        PA_STREAM_NOT_MONOTONIC | PA_STREAM_AUTO_TIMING_UPDATE |
        PA_STREAM_ADJUST_LATENCY;

    const char *dev_name = data->microphone ? NULL : data->sink_name;
    assert(pa_stream_connect_record(data->stream, dev_name, &buffer_attr, stream_flags) == 0);

    // Wait for the stream to be ready
    for(;;) {
        pa_stream_state_t stream_state = pa_stream_get_state(data->stream);
        assert(PA_STREAM_IS_GOOD(stream_state));
        if (stream_state == PA_STREAM_READY) break;
        pa_threaded_mainloop_wait(data->mainloop);
    }

    // Uncork the stream so it will start recording
    pa_stream_cork(data->stream, 0, stream_success_cb, data);
    for(;;) {
        if (pa_stream_is_corked(data->stream) == 0) break;
        pa_threaded_mainloop_wait(data->mainloop);
    }

    pa_threaded_mainloop_unlock(data->mainloop);

    return NULL;
}

static void context_state_cb(pa_context* context, void* userdata) {
    audio_thread_pa data = userdata;
    pa_threaded_mainloop_signal(data->mainloop, 0);
}

static void stream_state_cb(pa_stream *s, void *userdata) {
    audio_thread_pa data = userdata;
    pa_threaded_mainloop_signal(data->mainloop, 0);
}

static void stream_read_cb(pa_stream *stream, size_t nbytes, void *userdata) {
    audio_thread_pa data = (audio_thread_pa)userdata;

    ssize_t nbytes1 = (ssize_t)nbytes;
    while(nbytes1 > 0) {
        size_t count = nbytes1;
        const void *audio_data;
        int result = pa_stream_peek(stream, &audio_data, &count);

        if(count == 0) return;

        if(audio_data == NULL) {
            // hole
            pa_stream_drop(stream);
            return;
        }

        if(result != 0) {
            printf("pa_stream_peek error %d\n", result);
            return;
        }

        if(data->asr != NULL){
            asr_thread_enqueue_audio(data->asr, (short *)audio_data, count/2);
        }

        pa_stream_drop(stream);

        nbytes1 -= count;
    }
}

static void stream_success_cb(pa_stream *stream, int success, void *userdata) {
    audio_thread_pa data = userdata;
    pa_threaded_mainloop_signal(data->mainloop, 0);
}


audio_thread_pa create_audio_thread_pa(bool microphone, asr_thread asr) {
    audio_thread_pa data = calloc(1, sizeof(struct audio_thread_pa_i));

    data->microphone = microphone;
    data->asr = asr;
    data->sample_rate = asr_thread_samplerate(asr);

    return data;
}


void free_audio_thread_pa(audio_thread_pa thread){
    // Cork the stream
    pa_threaded_mainloop_lock(thread->mainloop);
    pa_stream_cork(thread->stream, 1, stream_success_cb, thread);
    for(;;) {
        if (pa_stream_is_corked(thread->stream) == 1) break;
        pa_threaded_mainloop_wait(thread->mainloop);
    }
    pa_threaded_mainloop_unlock(thread->mainloop);

    // Stop the main loop
    pa_threaded_mainloop_stop(thread->mainloop);

    // Disconnect and free everything
    pa_stream_disconnect(thread->stream);
    pa_stream_unref(thread->stream);

    pa_context_disconnect(thread->context);
    pa_context_unref(thread->context);

    pa_threaded_mainloop_free(thread->mainloop);

    free(thread->sink_name);
    free(thread->source_name);
}
