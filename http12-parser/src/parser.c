//
// Created by sw on 28.12.16.
//

#include <http_parser.h>
#include <zlib.h>
#include <sys/queue.h>
#include "parser.h"
#include "http1.h"
#include "http2.h"

int http_parser_open(logger *log, enum http_version version, enum connection_type type,
                     struct parser_callbacks *callbacks, void *attachment,
                     struct http_parser_context **p_context) {
    int r = 0;
    logger_log(log, LOG_LEVEL_TRACE, "http_parser_open()");
    *p_context = malloc(sizeof(struct http_parser_context));
    memset(*p_context, 0, sizeof(struct http_parser_context));
    struct http_parser_context *context = *p_context;
    context->version = version;
    context->type = type;
    context->log = log;
    context->callbacks = callbacks;
    context->attachment = attachment;
    switch (version) {
        case HTTP1:
            r = http1_parser_init(context);
            break;
        case HTTP2:
            r = http2_parser_init(context);
            break;
        default:
            r = PARSER_INVALID_ARGUMENT_ERROR;
            break;
    }

    logger_log(log, LOG_LEVEL_TRACE, "http_parser_open() returned with result: %d", r);
    return r;
}

int http_parser_input(struct http_parser_context *context, const char *data, size_t length) {
    switch (context->version) {
        case HTTP1:
            return http1_parser_input(context, data, length);
        case HTTP2:
            return http2_parser_input(context, data, length);
        default:
            return PARSER_INVALID_STATE_ERROR;
    }
}

int http_parser_close(struct http_parser_context *context) {
    switch (context->version) {
        case HTTP1:
            return http1_parser_close(context);
        case HTTP2:
            return http2_parser_close(context);
        default:
            return PARSER_INVALID_STATE_ERROR;
    }
}
