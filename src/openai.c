#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "openai.h"
#include "cJSON.h"

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    OpenAI_Response *response = (OpenAI_Response *)userp;

    if (response->response_text == NULL) {
        response->response_text = (char *)malloc(realsize + 1);
        if (response->response_text == NULL) {
            fprintf(stderr, "Not enough memory (malloc failed)\n");
            return 0;
        }
        memcpy(response->response_text, contents, realsize);
        response->response_text[realsize] = '\0';
    } else {
        char *ptr = (char *)realloc(response->response_text, strlen(response->response_text) + realsize + 1);
        if (ptr == NULL) {
            fprintf(stderr, "Not enough memory (realloc failed)\n");
            free(response->response_text);
            response->response_text = NULL;
            return 0;
        }
        response->response_text = ptr;
        strncat(response->response_text, (char *)contents, realsize);
    }
    return realsize;
}

bool openai_chat(OpenAI_Config *config, const char *model, OpenAI_Message *messages, int message_count, float temperature, OpenAI_Response *response) {
    CURL *curl;
    CURLcode res;
    bool result = false;

    response->response_text = NULL;
    response->id = NULL;
    response->object = NULL;
    response->created = 0;
    response->model = NULL;
    response->prompt_tokens = 0;
    response->completion_tokens = 0;
    response->total_tokens = 0;
    response->message_role = NULL;
    response->message_content = NULL;
    response->finish_reason = NULL;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        struct curl_slist *headers = NULL;

        headers = curl_slist_append(headers, "Content-Type: application/json");
        char auth_header[256];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", config->api_key);
        headers = curl_slist_append(headers, auth_header);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "model", model);

        cJSON *message_array = cJSON_CreateArray();
        for (int i = 0; i < message_count; i++) {
            cJSON *message_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(message_obj, "role", messages[i].role);
            cJSON_AddStringToObject(message_obj, "content", messages[i].content);
            cJSON_AddItemToArray(message_array, message_obj);
        }
        cJSON_AddItemToObject(root, "messages", message_array);
        cJSON_AddNumberToObject(root, "temperature", temperature);

        char *json_data = cJSON_PrintUnformatted(root);

        char *full_url = (char *)malloc(strlen(config->api_url) + strlen("/chat/completions") + 1);
        if (full_url) {
            strcpy(full_url, config->api_url);
            strcat(full_url, "/chat/completions");
            curl_easy_setopt(curl, CURLOPT_URL, full_url);
            free(full_url);
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status_code);

            cJSON *parsed_json = cJSON_Parse(response->response_text);
            if (parsed_json) {
                cJSON *id = cJSON_GetObjectItem(parsed_json, "id");
                cJSON *object = cJSON_GetObjectItem(parsed_json, "object");
                cJSON *created = cJSON_GetObjectItem(parsed_json, "created");
                cJSON *model_item = cJSON_GetObjectItem(parsed_json, "model");

                if (id && cJSON_IsString(id)) {
                    response->id = strdup(id->valuestring);
                }
                if (object && cJSON_IsString(object)) {
                    response->object = strdup(object->valuestring);
                }
                if (created && cJSON_IsNumber(created)) {
                    response->created = created->valueint;
                }
                if (model_item && cJSON_IsString(model_item)) {
                    response->model = strdup(model_item->valuestring);
                }

                cJSON *usage = cJSON_GetObjectItem(parsed_json, "usage");
                if (usage) {
                    cJSON *prompt_tokens = cJSON_GetObjectItem(usage, "prompt_tokens");
                    cJSON *completion_tokens = cJSON_GetObjectItem(usage, "completion_tokens");
                    cJSON *total_tokens = cJSON_GetObjectItem(usage, "total_tokens");

                    if (prompt_tokens && cJSON_IsNumber(prompt_tokens)) {
                        response->prompt_tokens = prompt_tokens->valueint;
                    }
                    if (completion_tokens && cJSON_IsNumber(completion_tokens)) {
                        response->completion_tokens = completion_tokens->valueint;
                    }
                    if (total_tokens && cJSON_IsNumber(total_tokens)) {
                        response->total_tokens = total_tokens->valueint;
                    }
                }

                cJSON *choices = cJSON_GetObjectItem(parsed_json, "choices");
                if (choices && cJSON_IsArray(choices)) {
                    cJSON *choice = cJSON_GetArrayItem(choices, 0);
                    if (choice) {
                        cJSON *message = cJSON_GetObjectItem(choice, "message");
                        if (message) {
                            cJSON *role = cJSON_GetObjectItem(message, "role");
                            cJSON *content = cJSON_GetObjectItem(message, "content");

                            if (role && cJSON_IsString(role)) {
                                response->message_role = strdup(role->valuestring);
                            }
                            if (content && cJSON_IsString(content)) {
                                response->message_content = strdup(content->valuestring);
                            }
                        }

                        cJSON *finish_reason = cJSON_GetObjectItem(choice, "finish_reason");
                        if (finish_reason && cJSON_IsString(finish_reason)) {
                            response->finish_reason = strdup(finish_reason->valuestring);
                        }
                    }
                }
                cJSON_Delete(parsed_json);
                result = true;
            }
        }

        curl_slist_free_all(headers);
        cJSON_Delete(root);
        free(json_data);
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return result;
}

void free_openai_response(OpenAI_Response *response) {
    if (response->response_text) {
        free(response->response_text);
        response->response_text = NULL;
    }
    if (response->id) {
        free(response->id);
        response->id = NULL;
    }
    if (response->object) {
        free(response->object);
        response->object = NULL;
    }
    if (response->model) {
        free(response->model);
        response->model = NULL;
    }
    if (response->message_role) {
        free(response->message_role);
        response->message_role = NULL;
    }
    if (response->message_content) {
        free(response->message_content);
        response->message_content = NULL;
    }
    if (response->finish_reason) {
        free(response->finish_reason);
        response->finish_reason = NULL;
    }
}