//
// Created by s.fionov on 12.01.17.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "http_header.h"
#include "util.h"

/*
 *  Utility methods definition:
 */

static int http_headers_add_field(struct http_headers *message, const char *name);

struct http_headers *http_headers_clone(const struct http_headers *source) {
    struct http_headers *headers = calloc(1, sizeof(struct http_headers));
    if (source->url != NULL)
        set_chars(&headers->url, source->url, strlen(source->url));
    if (source->status_string != NULL)
        set_chars(&headers->status_string, source->status_string, strlen(source->status_string));
    if (source->method != NULL)
        set_chars(&headers->method, source->method, strlen(source->method));
    headers->status_code = source->status_code;
    for (int i = 0; i < source->field_count; i++) {
        http_headers_add_empty_header_field(headers);
        set_chars(&http_headers_get_last_field(headers)->name, source->fields[i].name,
                  strlen(source->fields[i].name));
        set_chars(&http_headers_get_last_field(headers)->value, source->fields[i].value,
                  strlen(source->fields[i].value));
    }
    return headers;
}

int http_headers_set_method(struct http_headers *headers,
                            const char *method) {
    if (method == NULL) return 1;
    if (headers == NULL) return 1;
    if (headers->method) {
        free(headers->method);
        headers->method = NULL;
    }
    set_chars(&headers->method, method, strlen(method));
    return 0;
}

int http_headers_set_path(struct http_headers *headers,
                         const char *path) {
    if (path == NULL) return 1;
    if (headers == NULL) return 1;
    if (headers->url) {
        free(headers->url);
        headers->url = NULL;
    }
    set_chars(&headers->url, path, strlen(path));
    return 0;
}

int http_headers_set_status_string(struct http_headers *headers,
                            const char *status_string) {
    if (status_string == NULL) return 1;
    if (headers == NULL) return 1;
    if (headers->status_string) free(headers->status_string);
    set_chars(&headers->status_string, status_string, strlen(status_string));
    return 0;
}

int http_headers_set_status(struct http_headers *headers, int status_code) {
    if (headers == NULL) return 1;
    headers->status_code = status_code;
    return 0;
}

static int http_headers_add_field(struct http_headers *message, const char *name) {
    if (message == NULL || name == NULL) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncasecmp(message->fields[i].name, name, strlen(name)) == 0)
            return 1;
    }
    http_headers_add_empty_header_field(message);
    append_chars(&http_headers_get_last_field(message)->name, name, strlen(name));
    http_headers_get_last_field(message)->value = malloc(1);
    http_headers_get_last_field(message)->value[0] = '\0';
    return 0;
}

int http_headers_put_field(struct http_headers *headers,
                                  const char *name,
                                  const char *value) {
    if (headers == NULL || name == NULL) return 1;
    int found = 0;
    for (int i = 0; i < headers->field_count; i++) {
        if (strncasecmp(headers->fields[i].name, name, strlen(name)) == 0)
            found = 1;
    }
    if (!found) {
        if (http_headers_add_field(headers, name) != 0) {
            return 1;
        }
    }
    for (int i = 0; i < headers->field_count; i++) {
        if (strncasecmp(headers->fields[i].name, name, strlen(name)) == 0) {
            if (value != NULL) {
                set_chars(&headers->fields[i].value, value, strlen(value));
            } else {
                set_chars(&headers->fields[i].value, "", 0);
            }
            return 0;
        }
    }
    return  1;
}

const char *http_headers_get_field(const struct http_headers *message, const char *name) {
    if (message == NULL || name == NULL) return NULL;
    for (int i = 0; i < message->field_count; i++) {
        if (strncmp(message->fields[i].name, name, strlen(name)) == 0) {
            return message->fields[i].value;
        }
    }
    return NULL;
}

int http_headers_remove_field(struct http_headers *message, const char *name) {
    if (message == NULL || name == NULL) return 1;
    for (int i = 0; i < message->field_count; i++) {
        if (strncasecmp(message->fields[i].name, name, strlen(name)) == 0) {
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

/**
 * Create HTTP message
 * @param headers Pointer to variable where pointer to newly allocated message will be placed
 */
void http_headers_create(struct http_headers **headers) {
    *headers = malloc(sizeof(struct http_headers));
    memset(*headers, 0, sizeof(struct http_headers));
}

/**
 * Destroy HTTP message, including its state and field variables
 * @param headers Pointer to HTTP message
 */
void http_headers_free(struct http_headers *headers) {
    free(headers->url);
    free(headers->status_string);
    free(headers->method);
    for (int i = 0; i < headers->field_count; i++) {
        free(headers->fields[i].name);
        free(headers->fields[i].value);
    }
    free(headers->fields);
    free(headers);
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

const char *http_headers_get_method(struct http_headers *headers) {
    return headers->method;
}

const char *http_headers_get_path(struct http_headers *headers) {
    return headers->url;
}

int http_headers_get_status(struct http_headers *headers) {
    return headers->status_code;
}

const char *http_headers_get_status_string(struct http_headers *headers) {
    return headers->status_string;
}
