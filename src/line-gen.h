#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <april_api.h>
#include <adwaita.h>

#define AC_LINE_MAX 4096
#define AC_LINE_COUNT 2

struct line {
    char text[AC_LINE_MAX];
    size_t head;
    size_t len;
};

struct line_generator {
    size_t current_line;
    struct line lines[AC_LINE_COUNT];

    // Denotes the index within the active token array at which the line starts
    // If -1, means the active tokens don't reach that line yet
    ssize_t active_start_of_lines[AC_LINE_COUNT];

    char output[AC_LINE_MAX * AC_LINE_COUNT];
};

void line_generator_init(struct line_generator *lg);
void line_generator_update(struct line_generator *lg, size_t num_tokens, const AprilToken *tokens);
void line_generator_finalize(struct line_generator *lg);
void line_generator_set_text(struct line_generator *lg, GtkLabel *lbl);
