//
// Created by s.fionov on 23.12.16.
//

#ifndef HTTP_PARSER_PARSER_H
#define HTTP_PARSER_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include "logger.h"
#include "http_header.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum parser_type {
    HTTP1_REQUEST = 1,
    HTTP1_RESPONSE,
    HTTP2,
};

struct http_message {
    int stream_id;
    struct http_headers *headers;
};

struct http_parser_context {
    union {
        struct http1_parser_context *h1;
        struct http2_parser_context *h2;
    };
    /* Parser type */
    enum parser_type type;
    /* Logger */
    logger *log;
    /* Callbacks and attachment */
    struct parser_callbacks *callbacks;
    void *attachment;
};

struct parser_callbacks {
    /**
     * HTTP/1.1 headers callback
     * @param http_context HTTP context (already contains parsed headers)
     */
    void (*h1_headers)(struct http_headers *http_context);

    /**
     * HTTP/1.1 start of data callback
     * @param http_context HTTP context
     * @return 1 tells parser to decompress data, 0 tells to remain data uncompressed
     */
    bool (*h1_data_started)(struct http_headers *http_context);

    /**
     * HTTP/1.1 data callback
     * @param http_context HTTP context
     * @param data Processed HTTP/1.1 body data
     * @param length Data length
     */
    void (*h1_data)(struct http_message *http_context, const char *data, size_t length);

    /**
     * HTTP/1.1 end of data callback
     * @param http_context HTTP context
     */
    void (*h1_data_finished)(struct http_message *http_context);

    /**
     * HTTP/2 frame callback
     * @param ...
     */
    void (*h2_frame)();

    /**
     * HTTP/2 header callback
     * @param http_context HTTP context (already contains parsed headers)
     */
    void (*h2_headers)(struct http_message *http_context);

    /**
     * HTTP/2 start of data callback
     * @param http_context HTTP context
     * @return tells parser to decompress data, 0 tells to remain data uncompressed
     */
    bool (*h2_data_started)(struct http_message *http_context);

    /**
     * HTTP/2 data callback
     * @param http_context HTTP context
     * @param data Processed HTTP/2 stream data
     * @param length Data length
     */
    void (*h2_data)(struct http_message *http_context, const char *data, size_t length);

    /**
     * HTTP/2 end of data callback
     * @param http_context HTTP context
     */
    void (*h2_data_finished)(struct http_message *http_context, int is_reset);
};

/**
 * Parser error type
 */
typedef enum error_type {
    PARSER_OK = 0,
    PARSER_ALREADY_CONNECTED_ERROR = 101,
    PARSER_HTTP_PARSE_ERROR = 102,
    PARSER_ZLIB_ERROR = 103,
    PARSER_NULL_POINTER_ERROR = 104,
    PARSER_INVALID_ARGUMENT_ERROR = 105,
    PARSER_INVALID_STATE_ERROR = 106,
};


extern int http_parser_open(logger *log,
                            enum parser_type type,
                            struct parser_callbacks *callbacks,
                            void *attachment,
                            struct http_parser_context **p_context);

extern int http_parser_input(struct http_parser_context *context, const char *data, size_t length);

extern int http_parser_close(struct http_parser_context *context);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif //HTTP_PARSER_PARSER_H
