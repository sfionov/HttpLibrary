//
// Created by s.fionov on 12.01.17.
//

#ifndef HTTP_PARSER_HTTP2_H
#define HTTP_PARSER_HTTP2_H

#include <khash.h>
#include <nghttp2/nghttp2.h>
#include "parser.h"

#undef _U_
#ifdef __GNUC__
#define _U_ __attribute(unused)
#else
#define _U_
#endif /* __GNUC__ */


extern int http2_parser_init(struct http_parser_context *context);
extern int http2_parser_input(struct http_parser_context *context, const char *data, size_t length);
extern int http2_parser_close(struct http_parser_context *context);

struct h2p_frame_data
{
    const uint8_t *data; /* Not 0-terminated C-string! You must to use .size! */
    size_t        size;
};

typedef struct
{
    uint32_t          id;
    int               need_decode; /* Just 0 or 1 if need or not to decode. */
    struct headers   *headers;
    int32_t           nvlen;
} h2p_stream;

KHASH_MAP_INIT_INT(h2_streams_ht, h2p_stream*)

struct http2_parser_context {
    nghttp2_session         *session;
    khash_t(h2_streams_ht)  *streams;
    nghttp2_hd_deflater     *deflater;
    nghttp2_hd_inflater     *inflater;
};

/**
 * The frame types from HTTP/2 specification.
 * TODO: Do we need to define it separately
 */
typedef enum {
    H2P_FRAME_TYPE_DATA           = 0,
    H2P_FRAME_TYPE_HEADERS        = 0x01,
    H2P_FRAME_TYPE_PRIORITY       = 0x02,
    H2P_FRAME_TYPE_RST_STREAM     = 0x03,
    H2P_FRAME_TYPE_SETTINGS       = 0x04,
    H2P_FRAME_TYPE_PUSH_PROMISE   = 0x05,
    H2P_FRAME_TYPE_PING           = 0x06,
    H2P_FRAME_TYPE_GOAWAY         = 0x07,
    H2P_FRAME_TYPE_WINDOW_UPDATE  = 0x08,
    H2P_FRAME_TYPE_CONTINUATION   = 0x09,
    H2P_FRAME_TYPE_ALTSVC         = 0x0a
} h2p_frame_type;

#endif //HTTP_PARSER_HTTP2_H
