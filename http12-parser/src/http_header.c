//
// Created by s.fionov on 12.01.17.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "http_header.h"
#include "util.h"

#define HTTP_VERSION "HTTP/1.1"

/*
 *  Utility methods definition:
 */

struct http_headers *http_message_create() {
    return calloc(1, sizeof(struct http_headers));
}

struct http_headers *http_message_clone(const struct http_headers *source) {
    struct http_headers *message = http_message_create();
    if (source->url != NULL)
        set_chars(&message->url, source->url, strlen(source->url));
    if (source->status_string != NULL)
        set_chars(&message->status_string, source->status_string, strlen(source->status_string));
    if (source->method != NULL)
        set_chars(&message->method, source->method, strlen(source->method));
    message->status_code = source->status_code;
    for (int i = 0; i < source->field_count; i++) {
        http_headers_add_empty_header_field(message);
        set_chars(&http_headers_get_last_field(message)->name, source->fields[i].name,
                  strlen(source->fields[i].name));
        set_chars(&http_headers_get_last_field(message)->value, source->fields[i].value,
                  strlen(source->fields[i].value));
    }
    return message;
}

int http_message_set_method(struct http_headers *message,
                            const char *method, size_t length) {
    if (method == NULL) return 1;
    if (message == NULL) return 1;
    if (message->method) {
        free(message->method);
        message->method = NULL;
    }
    set_chars(&message->method, method, length);
    return 0;
}

int http_message_set_url(struct http_headers *message,
                         const char *url, size_t length) {
    if (url == NULL) return 1;
    if (message == NULL) return 1;
    if (message->url) {
        free(message->url);
        message->url = NULL;
    }
    set_chars(&message->url, url, length);
    return 0;
}

int http_message_set_status(struct http_headers *message,
                            const char *status, size_t length) {
    if (status == NULL) return 1;
    if (message == NULL) return 1;
    if (message->status_string) free(message->status_string);
    set_chars(&message->status_string, status, length);
    return 0;
}

int http_message_set_status_code(struct http_headers *message, int status_code) {
    if (message == NULL) return 1;
    message->status_code = status_code;
    return 0;
}

int http_message_set_header_field(struct http_headers *message,
                                  const char *name, size_t name_length,
                                  const char *value, size_t value_length) {
    if (message == NULL || name == NULL || name_length == 0 ||
        value == NULL || value_length == 0 ) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncmp(message->fields[i].name, name, name_length) == 0) {
            set_chars(&message->fields[i].value, value, value_length);
            return 0;
        }
    }
    return  1;
}

const char *http_message_get_header_field(const struct http_headers *message, const char *name,
                                          size_t name_length, size_t *p_value_length) {
    if (message == NULL || name == NULL || name_length == 0) return NULL;
    for (int i = 0; i < message->field_count; i++) {
        if (strncmp(message->fields[i].name, name, name_length) == 0) {
            *p_value_length = strlen(message->fields[i].value);
            return message->fields[i].value;
        }
    }
    return NULL;
}

int http_message_add_header_field(struct http_headers *message, const char *name, size_t length) {
    if (message == NULL || name == NULL || length == 0) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncasecmp(message->fields[i].name, name, length) == 0)
            return 1;
    }
    http_headers_add_empty_header_field(message);
    append_chars(&http_headers_get_last_field(message)->name,
                 name, length);
    http_headers_get_last_field(message)->value = calloc(1, 1);
    return 0;
}

int http_message_del_header_field(struct http_headers *message, const char *name, size_t length) {
    if (message == NULL || name == NULL || length == 0) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncasecmp(message->fields[i].name, name, length) == 0) {
            free (message->fields[i].name);
            free (message->fields[i].value);
            for (int j = i + 1; j < message->field_count; j++) {
                message->fields[j - 1].name = message->fields[j].name;
                message->fields[j - 1].value = message->fields[j].value;
            }
            message->field_count--;
            message->fields = realloc(message->fields, message->field_count * sizeof(struct http_header_field));
            return 0;
        }
    }
    return 1;
}

char *http_message_raw(const struct http_headers *message, size_t *p_length) {
    char *out_buffer;
    size_t length, line_length = 0;

    if (message == NULL) return NULL;

    if (message->status_string && message->status_code) {
        length = strlen(HTTP_VERSION) + strlen(message->status_string) + 7;
        out_buffer = malloc(length + 1);
        memset(out_buffer, 0, length + 1);
        snprintf(out_buffer, length + 1, "%s %u %s\r\n",
                 HTTP_VERSION, message->status_code,
                 message->status_string);
    } else {
        length = strlen(HTTP_VERSION) + strlen(message->url) +
                 strlen(message->method) + 4;
        out_buffer = malloc(length + 1);
        memset(out_buffer, 0, length + 1);
        snprintf(out_buffer, length + 1, "%s %s %s\r\n",
                 message->method, message->url, HTTP_VERSION);
    }

    for (int i = 0; i < message->field_count; i++) {
        line_length = strlen(message->fields[i].name) +
                      strlen(message->fields[i].value) + 4;
        out_buffer = realloc(out_buffer, length + line_length + 1);
        snprintf(out_buffer + length, line_length + 1, "%s: %s\r\n",
                 message->fields[i].name,
                 message->fields[i].value);
        length += line_length;
    }

    out_buffer = realloc(out_buffer, length + 3);
    snprintf(out_buffer + length, 3, "\r\n");
    length += 2;

    if (p_length != NULL) {
        *p_length = length;
    }
    return out_buffer;
}

/**
 * Create HTTP message
 * @param message Pointer to variable where pointer to newly allocated message will be placed
 */
void create_http_message(struct http_headers **message) {
    *message = malloc(sizeof(struct http_headers));
    memset(*message, 0, sizeof(struct http_headers));
}

/**
 * Destroy HTTP message, including its state and field variables
 * @param message Pointer to HTTP message
 */
void destroy_http_message(struct http_headers *message) {
    free(message->url);
    free(message->status_string);
    free(message->method);
    for (int i = 0; i < message->field_count; i++) {
        free(message->fields[i].name);
        free(message->fields[i].value);
    }
    free(message->fields);
    free(message);
}

/**
 * External interface for destroy_http_message()
 * @param message Pointer to HTTP message
 */
void http_message_free(struct http_headers *message) {
    destroy_http_message(message);
}

void http_headers_add_empty_header_field(struct http_headers *message) {
    message->field_count++;
    if (message->fields == NULL) {
        message->fields = malloc(sizeof(struct http_header_field));
        memset(message->fields, 0, sizeof(struct http_header_field));
    } else {
        message->fields = realloc(message->fields, message->field_count * sizeof(struct http_header_field));
        memset(&message->fields[message->field_count - 1], 0, sizeof(struct http_header_field));
    }
}

struct http_header_field *http_headers_get_last_field(struct http_headers *message) {
    return &message->fields[message->field_count - 1];
}
