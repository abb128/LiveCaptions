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

void history_init(void){
    // set timestamp for current session, etc

    active_session.timestamp = time(NULL);
    active_session.entries_count = 0;
    active_session.entries = NULL;
}


static struct history_entry *allocate_new_entry(size_t tokens_count) {
    active_session.entries_count += 1;
    active_session.entries = reallocarray(active_session.entries,
        active_session.entries_count, sizeof(struct history_entry));

    struct history_entry *entry = &active_session.entries[active_session.entries_count - 1];

    entry->tokens_count = tokens_count;
    entry->tokens = calloc(tokens_count, sizeof(struct history_token));

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


void save_current_history(const char *path){
    FILE *f = fopen(path, "w");

    fwrite(&active_session.timestamp, sizeof(active_session.timestamp), 1, f);
    fwrite(&active_session.entries_count, sizeof(active_session.entries_count), 1, f);
    for(size_t i=0; i<active_session.entries_count; i++){
        struct history_entry *entry = &active_session.entries[i];

        fwrite(&entry->timestamp, sizeof(entry->timestamp), 1, f);
        fwrite(&entry->tokens_count, sizeof(entry->tokens_count), 1, f);

        for(size_t j=0; j<entry->tokens_count; j++){
            struct history_token *token = &entry->tokens[j];

            fwrite(token, sizeof(struct history_token), 1, f);
        }
    }

    fclose(f);
}

// Currently replaces the active session with this one
void load_history_from(const char *path){
    FILE *f = fopen(path, "r");

    struct history_session *session = &active_session;
    fread(&session->timestamp, sizeof(session->timestamp), 1, f);
    fread(&session->entries_count, sizeof(session->entries_count), 1, f);

    session->entries = reallocarray(session->entries,
        session->entries_count,
        sizeof(struct history_entry)
    );

    for(size_t i=0; i<session->entries_count; i++){
        struct history_entry *entry = &session->entries[i];

        fread(&entry->timestamp, sizeof(entry->timestamp), 1, f);
        fread(&entry->tokens_count, sizeof(entry->tokens_count), 1, f);

        entry->tokens = reallocarray(entry->tokens,
            entry->tokens_count,
            sizeof(struct history_token)
        );

        for(size_t j=0; j<entry->tokens_count; j++){
            struct history_token *token = &entry->tokens[j];
            fread(token, sizeof(struct history_token), 1, f);
        }
    }

    fclose(f);
}



void export_history_as_text(const char *path) {

    char time_buff[512];

    FILE *f = fopen(path, "w");
    g_assert(f != NULL);

    struct history_session *session = &active_session;

    struct tm *tm = localtime(&session->timestamp);
    strftime(time_buff, 512, "%F", tm);

    fprintf(f, "    -[%s]-    ", time_buff);

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

    fclose(f);

    printf("Written to %s\n", path);
}
