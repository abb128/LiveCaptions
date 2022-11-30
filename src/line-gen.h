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