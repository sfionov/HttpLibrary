//
// Created by s.fionov on 12.01.17.
//

#ifndef HTTP_PARSER_HTTP2_H
#define HTTP_PARSER_HTTP2_H

#include <khash.h>
#include "parser.h"

extern int http2_parser_init(struct http_parser_context *context);
extern int http2_parser_input(struct http_parser_context *context, const char *data, size_t length);
extern int http2_parser_reset(struct http_parser_context *context);
extern int http2_parser_close(struct http_parser_context *context);

__KHASH_TYPE(stream_to_context, int, struct http_context *);

/**
 * http_parser_context contains fields for stream multiplexing
 */
struct http2_parser_context {
    khash_t(stream_to_context) context_map;
};


#endif //HTTP_PARSER_HTTP2_H
