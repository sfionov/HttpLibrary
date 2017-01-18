//
// Created by s.fionov on 16.01.17.
//

#ifndef HTTP_PARSER_UTIL_H
#define HTTP_PARSER_UTIL_H

#include <stdlib.h>
#include <string.h>

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

#endif //HTTP_PARSER_UTIL_H
