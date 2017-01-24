//
// Created by s.fionov on 16.01.17.
//

#ifndef HTTP_PARSER_UTIL_H
#define HTTP_PARSER_UTIL_H

#include <stdlib.h>
#include <string.h>
#include "http_header.h"

#undef _U_
#ifdef __GNUC__
#define _U_ __attribute__((unused))
#else
#define _U_
#endif /* __GNUC__ */

/**
 * Appends chars from character array `src' to null-terminated string `dst'
 * @param dst Pointer to null-terminated string (may be reallocated)
 * @param src Character array
 * @param len Length of character array
 */
static inline void append_chars(char **dst, const char *src, size_t len) {
    size_t old_len;
    if (*dst == NULL) {
        *dst = malloc(len + 1);
        old_len = 0;
    } else {
        old_len = strlen(*dst);
        *dst = realloc(*dst, old_len + len + 1);
    }
    memcpy(*dst + old_len, src, len);
    (*dst)[old_len + len] = 0;
}

/**
 * Copy characters from `src' character array to dst null-terminated string
 * @param dst Pointer to null-terminated string (may be reallocated)
 * @param src Character array
 * @param len Length of character array
 */
static inline void set_chars(char **dst, const char *src, size_t len) {
    *dst = realloc(*dst, len + 1);
    memcpy(*dst, src, len);
    (*dst)[len] = 0;
}

/**
 * Serializes HTTP headers structure to valid HTTP/1.1 message (request or response)
 * @param headers Pointer to HTTP headers structure
 * @param p_length Pointer to variable where length of output will be written
 * @return Non-null-terminated byte array containing HTTP/1.1 message
 */
char *http_headers_to_http1_message(const struct http_headers *headers, size_t *p_length);

#endif //HTTP_PARSER_UTIL_H
