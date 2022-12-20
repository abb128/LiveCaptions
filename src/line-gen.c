/* line-gen.c
 * This file contains the implementation for line_generator
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

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/mman.h>
#include <glib.h>
#include <stdbool.h>
#include <april_api.h>

#include <adwaita.h>

#include "line-gen.h"
#include "profanity-filter.h"

#define REL_LINE_IDX(HEAD, IDX) (4*AC_LINE_COUNT + (HEAD) + (IDX)) % AC_LINE_COUNT

static GSettings *settings = NULL;

void line_generator_init(struct line_generator *lg) {
    for(int i=0; i<AC_LINE_COUNT; i++){
        lg->active_start_of_lines[i] = -1;
    }

    lg->current_line = 0;
    lg->active_start_of_lines[0] = 0;

    if(settings == NULL) settings = g_settings_new("net.sapples.LiveCaptions");
}

static int line_generator_get_text_width(struct line_generator *lg, const char *text){
    pango_layout_set_width(lg->layout, -1);

    int width, height;
    pango_layout_set_text(lg->layout, text, strlen(text));
    pango_layout_get_size(lg->layout, &width, &height);

    return width / PANGO_SCALE;
}

#define MAX_TOKEN_SCRATCH 16
const char SWEAR_REPLACEMENT[] = " [__]";
void line_generator_update(struct line_generator *lg, size_t num_tokens, const AprilToken *tokens) {
    bool use_fade = g_settings_get_boolean(settings, "fade-text");
    bool use_filter = g_settings_get_boolean(settings, "filter-profanity");

    bool use_lowercase = !g_settings_get_boolean(settings, "text-uppercase");
    char token_scratch[MAX_TOKEN_SCRATCH];

    for(size_t i=0; i<AC_LINE_COUNT; i++){
        if(lg->active_start_of_lines[i] == -1) continue;
        size_t start_of_line = lg->active_start_of_lines[i];

        struct line *curr = &lg->lines[i];

        // reset for writing
        curr->text[0] = '\0';
        curr->head = 0;
        curr->len = 0;

        if(num_tokens == 0) continue;

        if(start_of_line >= num_tokens) {
            if(i == lg->current_line) {
                // oops... turns out our text isn't long enough for the new line
                // backtrack to the previous line
                lg->active_start_of_lines[lg->current_line] = -1;
                lg->current_line = REL_LINE_IDX(lg->current_line, -1);
                return line_generator_update(lg, num_tokens, tokens);
            } else {
                continue;
            }
        }


        ssize_t end = lg->active_start_of_lines[REL_LINE_IDX(i, 1)];
        if((end == -1) || (i == lg->current_line)) end = num_tokens;

        // print line
        for(size_t j=start_of_line; j<((size_t)end);) {
            int skipahead = 1;
            const char *token = tokens[j].token;
            if(use_lowercase){
                token = token_scratch;
                int i;
                for(i=0; i<strlen(tokens[j].token); i++) {
                    if((i+1) >= MAX_TOKEN_SCRATCH) {
                        printf("Token too long!\n");
                        break;
                    }

                    token_scratch[i] = tolower(tokens[j].token[i]);
                }
                
                token_scratch[i] = 0;
            }

            // filter current word, if applicable
            bool is_word_boundary = (token[0] == ' ');
            if(use_filter && is_word_boundary) {
                size_t skip = get_filter_skip(tokens, j, num_tokens);
                if(skip > 0) {
                    skipahead = skip;
                    token = SWEAR_REPLACEMENT;
                }
            }

            // skip if line is too long to safely write
            bool must_break = (curr->head > (AC_LINE_MAX - 256));
            if(must_break){
                printf("Must linebreak, but not active line. Leaving incomplete line...\n");
                break;
            }

            // break line if too long
            if(i == lg->current_line){
                curr->len += line_generator_get_text_width(lg, token);
                if(curr->len >= lg->max_text_width) {
                    size_t tgt_brk = j;
                    // find previous word boundary
                    while((tokens[tgt_brk].token[0] != ' ') && (tgt_brk > start_of_line)) tgt_brk--;

                    // if we backtracked all the way to the start of line, just give up and break here
                    if(tgt_brk == start_of_line) tgt_brk = j;

                    // line break
                    lg->current_line = REL_LINE_IDX(lg->current_line, 1);
                    lg->active_start_of_lines[lg->current_line] = tgt_brk;
                    return line_generator_update(lg, num_tokens, tokens);
                }
            }

            // write the actual line
            int alpha = (int)((tokens[j].logprob + 2.0) / 8.0 * 65536.0);
            alpha /= 2.0;
            alpha += 32768;
            if(alpha < 10000) alpha = 10000;
            if(alpha > 65535) alpha = 65535;

            if(use_fade)
                curr->head += sprintf(&curr->text[curr->head], "<span fgalpha=\"%d\">%s</span>", alpha, token);
            else
                curr->head += sprintf(&curr->text[curr->head], "%s", token);
            
            g_assert(curr->head < AC_LINE_MAX);

            j += skipahead;
        }
    }
}

void line_generator_finalize(struct line_generator *lg) {
    // fix when new line contains only like 1 token
    // ......

    // insert new line
    lg->current_line = REL_LINE_IDX(lg->current_line, 1);

    // reset active
    for(int i=0; i<AC_LINE_COUNT; i++) lg->active_start_of_lines[i] = -1;

    // set new line to start at 0
    lg->active_start_of_lines[lg->current_line] = 0;

    // clear new line
    lg->lines[lg->current_line].text[0] = '\0';
    lg->lines[lg->current_line].head = 0;
}

void line_generator_set_text(struct line_generator *lg, GtkLabel *lbl) {
    char *head = &lg->output[0];
    *head = '\0';

    for(int i=AC_LINE_COUNT-1; i>=0; i--) {
        struct line *curr = &lg->lines[REL_LINE_IDX(lg->current_line, -i)];
        head += sprintf(head, "%s", curr->text);

        if(i != 0) head += sprintf(head, "\n");
    }

    bool use_lowercase = !g_settings_get_boolean(settings, "text-uppercase");
    if(use_lowercase){
        for(int i=0; i<(AC_LINE_MAX * AC_LINE_COUNT); i++){
            if(lg->output[i] == '\0') break;
            lg->output[i] = tolower(lg->output[i]);
        }
    }

    gtk_label_set_markup(lbl, lg->output);
}
