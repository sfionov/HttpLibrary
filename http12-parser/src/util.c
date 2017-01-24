//
// Created by s.fionov on 16.01.17.
//

#include "util.h"

#include <stdio.h>

#include "http_header.h"

#define HTTP_VERSION "HTTP/1.1"

char *http_headers_to_http1_message(const struct http_headers *headers, size_t *p_length) {
    char *out_buffer;
    size_t length, line_length = 0;

    if (headers == NULL) return NULL;

    if (headers->status_string && headers->status_code) {
        length = strlen(HTTP_VERSION) + strlen(headers->status_string) + 7;
        out_buffer = malloc(length + 1);
        memset(out_buffer, 0, length + 1);
        snprintf(out_buffer, length + 1, "%s %u %s\r\n",
                 HTTP_VERSION, headers->status_code,
                 headers->status_string);
    } else {
        length = strlen(HTTP_VERSION) + strlen(headers->url) +
                 strlen(headers->method) + 4;
        out_buffer = malloc(length + 1);
        memset(out_buffer, 0, length + 1);
        snprintf(out_buffer, length + 1, "%s %s %s\r\n",
                 headers->method, headers->url, HTTP_VERSION);
    }

    for (int i = 0; i < headers->field_count; i++) {
        line_length = strlen(headers->fields[i].name) +
                      strlen(headers->fields[i].value) + 4;
        out_buffer = realloc(out_buffer, length + line_length + 1);
        snprintf(out_buffer + length, line_length + 1, "%s: %s\r\n",
                 headers->fields[i].name,
                 headers->fields[i].value);
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
