#ifndef OPENAI_H
#define OPENAI_H

#include <stdbool.h>

typedef struct {
    char *api_key;
    char *api_url;
} OpenAI_Config;

typedef struct {
    char *role;
    char *content;
} OpenAI_Message;

typedef struct {
    char *response_text;
    int status_code;
    char *id;
    char *object;
    long created;
    char *model;
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
    char *message_role;
    char *message_content;
    char *finish_reason;
} OpenAI_Response;

bool openai_chat(OpenAI_Config *config, const char *model, OpenAI_Message *messages, int message_count, float temperature, OpenAI_Response *response);

void free_openai_response(OpenAI_Response *response);

#endif // OPENAI_H