/* history.c
 *
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


#include <time.h>
#include <adwaita.h>
#include "history.h"

static struct history_session active_session = { 0 };
static struct past_history_sessions past_sessions = { 0 };

char default_history_file_v[1024] = { 0 };
char *default_history_file = NULL;

void history_init(void){
    // set timestamp for current session, etc
    active_session.timestamp = time(NULL);
    active_session.entries_count = 0;
    active_session.entries = NULL;

    past_sessions.num_sessions = 0;
    past_sessions.sessions = NULL;

    default_history_file = default_history_file_v;

    const char *data_dir = g_get_user_data_dir();
    sprintf(default_history_file_v, "%s/live-captions-history.bin", data_dir);

    printf("Save file: %s\n", default_history_file);
}


static struct history_entry *allocate_new_entry(size_t tokens_count) {
    active_session.entries_count += 1;
    active_session.entries = reallocarray(active_session.entries,
        active_session.entries_count, sizeof(struct history_entry));

    struct history_entry *entry = &active_session.entries[active_session.entries_count - 1];

    entry->tokens_count = tokens_count;

    if(tokens_count > 0)
        entry->tokens = calloc(tokens_count, sizeof(struct history_token));
    else
        entry->tokens = NULL;

    return entry;
}

void commit_tokens_to_current_history(const AprilToken *tokens,
                                      size_t tokens_count)
{
    struct history_entry *entry = allocate_new_entry(tokens_count);

    entry->timestamp = time(NULL);

    for(size_t i=0; i<tokens_count; i++){
        struct history_token *token = &entry->tokens[i];

        if(strlen(tokens[i].token) >= HISTORY_TOKEN_MAX_CHARS){
            printf("Token %s is too long! (%d)\n", tokens[i].token, strlen(tokens[i].token));
            g_assert(false);
        }

        strcpy(&token->token[0], tokens[i].token);
        token->logprob = tokens[i].logprob;
        token->flags   = tokens[i].flags;
    }
}

void save_silence_to_history(void){
    struct history_entry *entry = allocate_new_entry(0);
    entry->timestamp = time(NULL);
}


static void write_session_to_file(FILE *f, const struct history_session *session) {
    fwrite(&session->timestamp, sizeof(session->timestamp), 1, f);
    fwrite(&session->entries_count, sizeof(session->entries_count), 1, f);
    for(size_t i=0; i<session->entries_count; i++){
        struct history_entry *entry = &session->entries[i];

        fwrite(&entry->timestamp, sizeof(entry->timestamp), 1, f);
        fwrite(&entry->tokens_count, sizeof(entry->tokens_count), 1, f);

        for(size_t j=0; j<entry->tokens_count; j++){
            struct history_token *token = &entry->tokens[j];

            fwrite(token, sizeof(struct history_token), 1, f);
        }
    }
}

void save_current_history(const char *path){
    FILE *f = fopen(path, "w");

    bool write_active_session = active_session.entries_count > 0;

    size_t num_sessions_to_write = past_sessions.num_sessions + (write_active_session ? 1 : 0);
    fwrite(&num_sessions_to_write, sizeof(num_sessions_to_write), 1, f);

    for(size_t i=0; i<past_sessions.num_sessions; i++){
        write_session_to_file(f, &past_sessions.sessions[i]);
    }

    if(write_active_session)
        write_session_to_file(f, &active_session);

    fclose(f);
}


static void read_session_from_file(FILE *f, struct history_session *session) {
    fread(&session->timestamp, sizeof(session->timestamp), 1, f);
    fread(&session->entries_count, sizeof(session->entries_count), 1, f);

    session->entries = calloc(
        session->entries_count,
        sizeof(struct history_entry)
    );

    for(size_t i=0; i<session->entries_count; i++){
        struct history_entry *entry = &session->entries[i];

        fread(&entry->timestamp, sizeof(entry->timestamp), 1, f);
        fread(&entry->tokens_count, sizeof(entry->tokens_count), 1, f);

        if(entry->tokens_count == 0){
            entry->tokens = NULL;
            continue;
        }

        entry->tokens = calloc(
            entry->tokens_count,
            sizeof(struct history_token)
        );

        for(size_t j=0; j<entry->tokens_count; j++){
            struct history_token *token = &entry->tokens[j];
            fread(token, sizeof(struct history_token), 1, f);
        }
    }
}

void load_history_from(const char *path){
    FILE *f = fopen(path, "r");

    if(f == NULL) {
        printf("fopen %s returned NULL\n", path);
        return;
    }

    size_t num_sessions_in_file = 0;
    fread(&num_sessions_in_file, sizeof(num_sessions_in_file), 1, f);

    past_sessions.num_sessions = num_sessions_in_file;
    past_sessions.sessions = calloc(past_sessions.num_sessions, sizeof(struct history_session));
    
    for(size_t i=0; i<num_sessions_in_file; i++){
        read_session_from_file(f, &past_sessions.sessions[i]);
    }

    fclose(f);
}


static void export_session_into_text(FILE *f, const struct history_session *session) {
    char time_buff[512];

    struct tm *tm = localtime(&session->timestamp);
    strftime(time_buff, 512, "%F | %H:%M", tm);

    fprintf(f, "    -[ %s ]-    ", time_buff);

    for(size_t i=0; i<session->entries_count; i++){
        struct history_entry *entry = &session->entries[i];

        tm = localtime_r(&entry->timestamp, tm);
        strftime(time_buff, 512, "%T", tm);

        fprintf(f, "\n(%s) - ", time_buff);

        for(size_t j=0; j<entry->tokens_count; j++){
            fprintf(f, "%s", entry->tokens[j].token);
        }
    }

    fprintf(f, "\n\n");
}

void export_history_as_text(const char *path) {
    // TODO: Apply current settings for filtering, capitalization, etc
    
    FILE *f = fopen(path, "w");
    g_assert(f != NULL);

    for(size_t i=0; i<past_sessions.num_sessions; i++){
        if(past_sessions.sessions[i].entries_count == 0) continue;

        export_session_into_text(f, &past_sessions.sessions[i]);
    }

    if(active_session.entries_count > 0)
        export_session_into_text(f, &active_session);

    fclose(f);
}

const struct history_session *get_history_session(size_t idx) {
    if(idx == 0) return &active_session;

    ssize_t i = ((ssize_t)past_sessions.num_sessions - (ssize_t)idx);
    if(i < 0) return NULL;

    return &past_sessions.sessions[i];
}