/* history.h
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

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <april_api.h>
#include <adwaita.h>

#define HISTORY_TOKEN_MAX_CHARS 32
#define HISTORY_MAX_TOKENS 256

extern char *default_history_file;


// A single token. The token text is inline for serialization simplicity
struct history_token {
    char token[HISTORY_TOKEN_MAX_CHARS]; // should this be a dynamic array?
    float logprob;
    AprilTokenFlagBits flags;
};

// A single history entry containing a collection of tokens
struct history_entry {
    time_t timestamp;
    size_t tokens_count;
    struct history_token *tokens;
};

// A Live Captions session
struct history_session {
    time_t timestamp;
    size_t entries_count;
    struct history_entry *entries;
};

// List of past sessions
struct past_history_sessions {
    size_t num_sessions;
    struct history_session *sessions;
};

// Use static global variables for simplicity

// Initialize history
void history_init(void);

// Every time finalized, commit to list of history_entry
void commit_tokens_to_current_history(const AprilToken *tokens,
                                      size_t tokens_count);


// Serialize/Deserialize list of history_entry
void save_current_history(const char *path);
void load_history_from(const char *path);

// Convert to text file
void export_history_as_text(const char *path);
/*
Format:

    -[2022-02-02 | 16:47]-
(16:47:04) - this is some text hello
(16:47:37) - hello hello hello hello hello hello hello hello
(16:47:52) - some text here some text here


    -[2022-02-02 | 16:48]-
(16:48:32) - some text here some text here
(16:48:57) - hello hello hello hello hello hello hello hello


*/

// Display in a history window. Make sure it's copyable
// ?

// 0 returns the active session
// 1 returns the previous session
// 2 returns the one prior to the previous
// ...
// returns NULL once reached the first session
const struct history_session *get_history_session(size_t idx);