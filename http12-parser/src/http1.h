//
// Created by s.fionov on 12.01.17.
//

#ifndef HTTP_PARSER_HTTP1_H
#define HTTP_PARSER_HTTP1_H

#include <stddef.h>
#include "parser.h"

extern int http1_parser_init(struct http_parser_context *context, int is_response);
extern int http1_parser_input(struct http_parser_context *context, const char *data, size_t length);
extern int http1_parser_reset(struct http_parser_context *context);
extern int http1_parser_close(struct http_parser_context *context);

#endif //HTTP_PARSER_HTTP1_H
