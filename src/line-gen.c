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

#define REL_LINE_IDX(HEAD, IDX) (4*AC_LINE_COUNT + (HEAD) + (IDX)) % AC_LINE_COUNT

void line_generator_init(struct line_generator *lg) {
    for(int i=0; i<AC_LINE_COUNT; i++){
        lg->active_start_of_lines[i] = -1;
    }

    lg->current_line = 0;
    lg->active_start_of_lines[0] = 0;
}

void line_generator_update(struct line_generator *lg, size_t num_tokens, const AprilToken *tokens) {
    for(int i=0; i<AC_LINE_COUNT; i++){
        if(lg->active_start_of_lines[i] == -1) continue;

        struct line *curr = &lg->lines[i];

        // reset for writing
        curr->text[0] = '\0';
        curr->head = 0;
        curr->len = 0;

        if(lg->active_start_of_lines[i] >= num_tokens) {
            printf("%d more tokens than exist %d!\n", lg->active_start_of_lines[i], num_tokens);
            if(i == lg->current_line) {
                // oops... turns out our text isn't long enough for the new line
                // backtrack to the previous line
                lg->active_start_of_lines[lg->current_line] = -1;
                lg->current_line = REL_LINE_IDX(lg->current_line, -1);
                return line_generator_update(lg, num_tokens, tokens); // TODO?
            } else {
                continue;
            }
        }


        int end = lg->active_start_of_lines[REL_LINE_IDX(i, 1)];
        if((end == -1) || (i == lg->current_line)) end = num_tokens;

        // print line
        for(int j=lg->active_start_of_lines[i]; j<end; j++) {
            bool can_break_nicely = (curr->len > 48) && (tokens[j].token[0] == ' ') && (tokens[j].logprob > -1.0f);
            bool must_break = (curr->head > (AC_LINE_MAX - 256));
            if((i == lg->current_line) && (can_break_nicely  || must_break)) {
                // line break
                lg->current_line = REL_LINE_IDX(lg->current_line, 1);
                lg->active_start_of_lines[lg->current_line] = j;
                return line_generator_update(lg, num_tokens, tokens); // TODO?
            }

            if(must_break){
                printf("Must linebreak, but not active line. Leaving incomplete line...\n");
                break;
            }

            int alpha = (int)((tokens[j].logprob + 2.0) / 8.0 * 65536.0);
            alpha /= 2.0;
            alpha += 32768;
            if(alpha < 10000) alpha = 10000;
            if(alpha > 65535) alpha = 65535;
            curr->head += sprintf(&curr->text[curr->head], "<span fgalpha=\"%d\">%s</span>", alpha, tokens[j].token);
            g_assert(curr->head < AC_LINE_MAX);

            curr->len += strlen(tokens[j].token);
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

    for(int i=0; i<(AC_LINE_MAX * AC_LINE_COUNT); i++){
        if(lg->output[i] == '\0') break;
        lg->output[i] = tolower(lg->output[i]);
    }

    gtk_label_set_markup(lbl, lg->output);
}