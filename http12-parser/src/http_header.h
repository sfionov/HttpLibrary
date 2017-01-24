//
// Created by sw on 26.12.16.
//

#ifndef HTTP_PARSER_HTTP_HEADER_H
#define HTTP_PARSER_HTTP_HEADER_H

#ifdef __cplusplus
extern "C" {
#endif

struct http_header_field {
    char *name;
    char *value;
};

/**
 * List of HTTP headers
 * Pseudo-header fields are first
 * HTTP/1.1 status line fields are represented with the following pseudo-header fields:
 * Request: <:method> <:path> HTTP/1.1
 * Response: HTTP/1.1 <:status> <:status-message>
 */
struct http_headers {
    int field_count;
    int allocated_field_count;
    struct http_header_field *fields;
    char *url;
    int status_code;
    char *status_string;
    char *method;
};

extern struct http_headers *http_headers_clone(const struct http_headers *source);
extern const char *http_headers_get_field(const struct http_headers *headers, const char *name);
extern int http_headers_put_field(struct http_headers *headers, const char *name, const char *value);
extern int http_headers_remove_field(struct http_headers *headers, const char *name);

/*
 * HTTP/1.1 compatibility functions
 */

extern const char *http_headers_get_method(struct http_headers *headers);
extern int http_headers_set_method(struct http_headers *headers, const char *method);
extern const char *http_headers_get_path(struct http_headers *headers);
extern int http_headers_set_path(struct http_headers *headers, const char *path);
extern int http_headers_get_status(struct http_headers *headers);
extern int http_headers_set_status(struct http_headers *headers, int code);
extern const char *http_headers_get_status_string(struct http_headers *headers);
extern int http_headers_set_status_string(struct http_headers *headers, const char *status_string);

/**
 * @defgroup Utility methods for fast writing to tail of headers structure
 * @{
 */

/**
 * Allocate place for next HTTP header field
 * @param message Pointer to HTTP message
 */
extern void http_headers_add_empty_header_field(struct http_headers *message);

/**
 * Get last HTTP header field
 * @param message Pointer to HTTP message
 * @return Pointer to last HTTP header field
 */
extern struct http_header_field *http_headers_get_last_field(struct http_headers *message);

/**
 * @}
 */

/**
 * Create HTTP headers structure
 * @param headers Pointer to variable to write pointer to new HTTP headers structure
 */

extern void http_headers_create(struct http_headers **headers);

/**
 * External interface for destroy_http_message()
 * @param message Pointer to HTTP message
 */
extern void http_headers_free(struct http_headers *message);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif //HTTP_PARSER_HTTP_HEADER_H