//
// Created by s.fionov on 12.01.17.
//

#include <zlib.h>
#include <http_parser.h>
#include "parser.h"
#include "http1.h"
#include "util.h"
#include "http_header.h"

#define PARSER_LOG(args...) logger_log(parser_ctx->log, args)
#define CTX_LOG(args...) logger_log(context->parser_ctx->log, args)
#define HTTP_VERSION "HTTP/1.1"
#define ZLIB_DECOMPRESS_CHUNK_SIZE 262144

/**
 * Content-Encoding enum type.
 * Encoding supported by this library - `identity', `deflate' and `gzip'
 */
typedef enum {
    CONTENT_ENCODING_IDENTITY = 0,
    CONTENT_ENCODING_DEFLATE = 1,
    CONTENT_ENCODING_GZIP = 2
} content_encoding_t;

typedef struct parser_context parser_context;

/*
 * Connection context structure
 */
struct http1_parser_context { /* structure has name for queue.h macros */
    // Connection id
    long                    id;
    // Current error message
    char                    error_message[256];
    // Body callback error
    enum error_type         body_callback_error;
    // Parser callbacks
    struct parser_callbacks *callbacks;
    // Pointer to Node.js http_parser implementation
    http_parser             *parser;
    // Pointer to parser settings
    http_parser_settings    *settings;
    // Pointer to message which is currently being constructed
    struct http_context     *http_message;
    struct http_headers     *message;
    // Pointer to parent context
    parser_context          *parser_ctx;

    // State flags:
    // Message construction is done
    size_t                  done;
    // We are currently in field (after retrieveing field name and before reteiving field value)
    int                     in_field;
    /* Flag if message have body (Content-length is more than zero, body can yet be empty if
     * consists to one empty chunk)
     */
    int                     have_body;
    // We are currently in body and body decoding started (if message body needs any kind of decoding)
    int                     body_started;
    // Decode is needed flag
    int                     need_decode;
    // Content-Encoding of body - identity (no encoding), deflate, gzip. Determined from headers.
    content_encoding_t      content_encoding;
    // Zlib decode input buffer. Contains tail on previous input buffer which can't be processed right now
    char                    *decode_in_buffer;
    // Zlib decode output buffer. Contains currently decompressed data
    char                    *decode_out_buffer;
    // Zlib stream
    z_stream                zlib_stream;
};

/*
 * Functions for body decompression.
 * Since decompression of one chunk of data may result on more than one output chunk, passing callback is needed.
 */
/*
 * Body data callback. Usually it is parser_callbacks.http_request_body_data()
 */
typedef void (*body_data_callback)(struct http_context *context, const char *at, size_t length);
/*
 * Initializes zlib stream for current HTTP message body.
 * Used internally by http_parser_on_body().
 */
static int message_inflate_init(struct http1_parser_context *context);
/*
 * Inflate current input buffer and call body_data_callback for each decompressed chunk
 */
static int message_inflate(struct http1_parser_context *context, const char *data, size_t length, body_data_callback body_data);
/*
 * Deinitializes zlib stream for current HTTP message body.
 */
static int message_inflate_end(struct http1_parser_context *context);

/*
 * Other utility functions.
 */
/**
 * Throw an error for current connection context. It calls parse_error callback with given error type and message
 * @param context Connection context
 * @param direction Transfer direction
 * @param msg Error message (may be null or empty)
 */
static void set_error(struct http1_parser_context *context, const char *msg);

/*
 *  Node.js http_parser's callbacks (parser->settings):
 */
/* For in-callback using only! */

static content_encoding_t get_content_encoding(struct http1_parser_context *context);

void parser_reset(struct http1_parser_context *context);

/**
 * Context by id hash map implementation
 */
#define HASH_SIZE 63

struct context_by_id {
    struct http1_parser_context *tqh_first;
    struct http1_parser_context **tqh_last;
};
struct parser_context {
    struct context_by_id context_by_id_hash[HASH_SIZE];
    int context_by_id_hash_initialized;
    logger *log;
};

/*
 *  Internal callbacks:
 */
int http_parser_on_message_begin(http_parser *parser) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_begin(parser=%p)", parser);
    create_http_message(&context->message);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_begin() returned %d", 0);
    return 0;
}

int http_parser_on_url(http_parser *parser, const char *at, size_t length) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_url(parser=%p, at=%.*s)", parser, (int) length, at);
    struct http_headers *message = context->message;
    if (at != NULL && length > 0) {
        append_chars(&message->url, at, length);
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_url() returned %d", 0);
    return 0;
}

int http_parser_on_status(http_parser *parser, const char *at, size_t length) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_status(parser=%p, at=%.*s)", parser, (int) length, at);
    struct http_headers *message = context->message;
    if (at != NULL && length > 0) {
        append_chars(&message->status_string, at, length);
        message->status_code = parser->status_code;
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_status() returned %d", 0);
    return 0;
}

int http_parser_on_header_field(http_parser *parser, const char *at, size_t length) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_field(parser=%p, at=%.*s)", parser, (int) length, at);
    struct http_headers *message = context->message;
    if (at != NULL && length > 0) {
        if (!context->in_field) {
            context->in_field = 1;
            http_headers_add_empty_header_field(message);
        }
        append_chars(&http_headers_get_last_field(message)->name, at, length);
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_field() returned %d", 0);
    return 0;
}

int http_parser_on_header_value(http_parser *parser, const char *at, size_t length) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_value(parser=%p, at=%.*s)", parser, (int) length, at);
    struct http_headers *message = context->message;
    context->in_field = 0;
    if (at != NULL && length > 0) {
        append_chars(&http_headers_get_last_field(message)->value, at, length);
    } else {
        http_headers_get_last_field(message)->value = calloc(1, 1);
    }
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_header_value() returned %d", 0);
    return 0;
}

int http_parser_on_headers_complete(http_parser *parser) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_headers_complete(parser=%p)", parser);
    struct http_headers *message = context->message;
    const char *method;
    int skip = 0;
    switch (parser->type) {
        case HTTP_REQUEST:
            method = http_method_str((enum http_method) parser->method);
            set_chars(&message->method, method, strlen(method));
            break;
        case HTTP_RESPONSE:
            break;
        default:
            break;
    }

    context->callbacks->h1_headers(context->message);

    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_headers_complete() returned %d", skip);
    return skip;
}

int http_parser_on_body(http_parser *parser, const char *at, size_t length) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_body(parser=%p, length=%d)", parser, (int) length);
    context->have_body = 1;
    enum error_type r = PARSER_OK;
    switch (parser->type) {
        case HTTP_REQUEST:
        case HTTP_RESPONSE:
            break;
        default:
            r = PARSER_INVALID_ARGUMENT_ERROR;
            goto out;
    }

    if (context->body_started == 0) {
        context->need_decode = context->callbacks->h1_data_started(context->message);
        if (context->need_decode) {
            if (message_inflate_init(context) != 0) {
                set_error(context, context->zlib_stream.msg);
                r = PARSER_ZLIB_ERROR;
                goto out;
            }
        }
        context->body_started = 1;
    }
    if (context->content_encoding == CONTENT_ENCODING_IDENTITY) {
        context->callbacks->h1_data(context->message, at, length);
    } else {
        if (message_inflate(context, at, length, context->callbacks->h1_data) != 0) {
            set_error(context, context->zlib_stream.msg);
            return PARSER_ZLIB_ERROR;
        }
    }

    out:
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_body() returned %d", r);
    context->body_callback_error = r;
    return r;
}

static void set_error(struct http1_parser_context *context, const char *msg) {
    snprintf(context->error_message, 256, "%s", msg ? msg : "");
}

/**
 * Initialize Zlib stream depending on Content-Encoding (gzip/deflate)
 * @param context Connection context
 */
static int message_inflate_init(struct http1_parser_context *context) {
    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate_init()");
    context->content_encoding = get_content_encoding(context);
    if (!context->need_decode || context->content_encoding == CONTENT_ENCODING_IDENTITY) {
        // Uncompressed
        context->decode_in_buffer = NULL;
        context->decode_out_buffer = NULL;
        return 0;
    }

    memset(&context->zlib_stream, 0, sizeof(z_stream));
    context->decode_in_buffer = malloc(ZLIB_DECOMPRESS_CHUNK_SIZE);
    memset(context->decode_in_buffer, 0, ZLIB_DECOMPRESS_CHUNK_SIZE);
    context->decode_out_buffer = malloc(ZLIB_DECOMPRESS_CHUNK_SIZE);
    memset(context->decode_out_buffer, 0, ZLIB_DECOMPRESS_CHUNK_SIZE);

    int r;
    switch (context->content_encoding) {
        case CONTENT_ENCODING_DEFLATE:
            r = inflateInit(&context->zlib_stream);
            break;
        case CONTENT_ENCODING_GZIP:
            r = inflateInit2(&context->zlib_stream, 16 + MAX_WBITS);
            break;
        default:
            r = 0;
            break;
    }

    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate_init() returned %d", r);
    return r;
}

static content_encoding_t get_content_encoding(struct http1_parser_context *context) {
    struct http_headers *message = context->message;
    char *field_name = "Content-Encoding";
    const char *value = http_headers_get_field(message, field_name);
    size_t value_length = value ? strlen(value) : 0;
    if (!strncasecmp(value, "gzip", value_length) || !strncasecmp(value, "x-gzip", value_length)) {
        return CONTENT_ENCODING_GZIP;
    } else if (!strncasecmp(value, "deflate", value_length)) {
        return CONTENT_ENCODING_DEFLATE;
    } else {
        return CONTENT_ENCODING_IDENTITY;
    }
}

/**
 * Decompress stream
 * @param context Connection context
 * @param data Input data
 * @param length Input data length
 * @param body_data Data callback function
 * @return 0 if data is successfully decompressed, 1 in case of error
 */
static int message_inflate(struct http1_parser_context *context, const char *data, size_t length, body_data_callback body_data) {
    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate(data=%p, length=%d)", data, (int) length);
    int result;
    int r = 0;

    if (context->decode_out_buffer == NULL) {
        r = 1;
        // Compression stream was not initialized
        goto finish;
    }

    // If we have a data in input buffer, append new data to it, otherwise process `data' as input buffer
    if (context->zlib_stream.avail_in > 0) {
        context->zlib_stream.next_in = (Bytef *) context->decode_in_buffer;
        if (context->zlib_stream.avail_in + length > ZLIB_DECOMPRESS_CHUNK_SIZE) {
            result = Z_BUF_ERROR;
            goto error;
        }
        memcpy(context->decode_in_buffer + context->zlib_stream.avail_in, data, length);
        context->zlib_stream.avail_in += (uInt) length;
    } else {
        context->zlib_stream.next_in = (Bytef *) data;
        context->zlib_stream.avail_in = (uInt) length;
    }

    int old_avail_in; // Check if we made a progress
    do {
        old_avail_in = context->zlib_stream.avail_in;
        context->zlib_stream.avail_out = ZLIB_DECOMPRESS_CHUNK_SIZE;
        context->zlib_stream.next_out = (Bytef *) context->decode_out_buffer;
        result = inflate(&context->zlib_stream, 0);
        if (result == Z_OK || result == Z_STREAM_END) {
            size_t processed = ZLIB_DECOMPRESS_CHUNK_SIZE - context->zlib_stream.avail_out;
            if (processed > 0) {
                // Call callback function for each decompressed block
                body_data(context, context->decode_out_buffer, processed);
            }
        }
        if (result != Z_OK) {
            goto error;
        }
    } while (context->zlib_stream.avail_in > 0 && old_avail_in != context->zlib_stream.avail_in);

    // Move unprocessed tail to the start of input buffer
    if (context->zlib_stream.avail_in) {
        memcpy(context->decode_in_buffer, context->zlib_stream.next_in, context->zlib_stream.avail_in);
    }

    goto finish;

    error:
    message_inflate_end(context); /* result ignored */
    if (result != Z_STREAM_END) {
        fprintf(stderr, "Decompression error: %d\n", result);
        r = 1;
    }

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "message_inflate() returned %d", r);
    return r;
}

/**
 * Deinitialize stream compression structures
 * @param context
 */
static int message_inflate_end(struct http1_parser_context *context) {
    int result = inflateEnd(&context->zlib_stream);
    free(context->decode_in_buffer);
    context->decode_in_buffer = NULL;
    free(context->decode_out_buffer);
    context->decode_out_buffer = NULL;
    return result;
}

int http_parser_on_message_complete(http_parser *parser) {
    struct http1_parser_context *context = ((struct http1_parser_context *) parser->data);
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_complete(parser=%p)", parser);
    struct http_headers *message = context->message;
    if (context->have_body) {
        context->callbacks->h1_data_finished(context->http_message);
    }

    parser_reset(context);

    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_on_message_complete() returned %d", 0);
    return 0;
}

int http_parser_on_chunk_header(http_parser *parser) {
    // ignore
    return 0;
}

int http_parser_on_chunk_complete(http_parser *parser) {
    // ignore
    return 0;
}


http_parser_settings _settings = {
        .on_message_begin       = http_parser_on_message_begin,
        .on_url                 = http_parser_on_url,
        .on_status              = http_parser_on_status,
        .on_header_field        = http_parser_on_header_field,
        .on_header_value        = http_parser_on_header_value,
        .on_headers_complete    = http_parser_on_headers_complete,
        .on_body                = http_parser_on_body,
        .on_message_complete    = http_parser_on_message_complete,
        .on_chunk_header        = http_parser_on_chunk_header,
        .on_chunk_complete      = http_parser_on_chunk_complete
};

/*
 *  API implementation
 */

int http1_parser_init(struct http_parser_context *h12_context, int is_response) {
    logger_log(h12_context->log, LOG_LEVEL_TRACE, "http1_parser_init(context=%p)", h12_context);
    int r = 0;

    struct http1_parser_context *context = malloc(sizeof(struct http1_parser_context));

    context->settings = &_settings;
    context->parser = malloc(sizeof(http_parser));

    context->parser->data = context;

    parser_reset(context);

    h12_context->h1 = context;

    logger_log(h12_context->log, LOG_LEVEL_TRACE, "parser_connect() returned %d", r);
    return r;
}

void parser_reset(struct http1_parser_context *context) {
    if (context->decode_out_buffer != NULL) {
        message_inflate_end(context);
    }

    if (context->message != NULL) {
        destroy_http_message(context->message);
    }
    context->message = 0;
    context->have_body = 0;
    context->body_started = 0;
    context->content_encoding = CONTENT_ENCODING_IDENTITY;

    /* Re-init parser before next message. */
    http_parser_init(context->parser, HTTP_BOTH);
}

#define INPUT_LENGTH_AT_ERROR 1

int parser_input(struct http_parser_context *h12_context, const char *data, size_t length) {
    logger_log(h12_context->log, LOG_LEVEL_TRACE, "parser_input(context=%p, len=%d)", h12_context, (int) length);
    struct http1_parser_context *context = h12_context->h1;
    // TODO: this is wrong, null bytes are allowed in content
    context->done = 0;

    if (HTTP_PARSER_ERRNO(context->parser) != HPE_OK || context->parser->type == HTTP_BOTH) {
        http_parser_init(context->parser, h12_context->type == HTTP1_REQUEST ? HTTP_REQUEST : HTTP_RESPONSE);
    }

    context->done = http_parser_execute(context->parser, context->settings,
                                        data, length);

    int r = 0;
    while (context->done < length)
    {
        if (HTTP_PARSER_ERRNO(context->parser) != HPE_OK) {
            enum http_errno http_parser_errno = HTTP_PARSER_ERRNO(context->parser);
            if (http_parser_errno != HPE_CB_body) {
                // If body data callback fails, then get saved error from structure, don't overwrite
                set_error(context, http_errno_description(http_parser_errno));
                r = PARSER_HTTP_PARSE_ERROR;
            } else {
                r = context->body_callback_error;
            }

            // http_parser_init(context->parser, direction == DIRECTION_OUT ? HTTP_REQUEST : HTTP_RESPONSE);
            goto finish;
        }

        // Input was not fully processed, turn on slow mode
        http_parser_execute(context->parser, context->settings,
                            data + context->done, INPUT_LENGTH_AT_ERROR);
        context->done += INPUT_LENGTH_AT_ERROR;
    }

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "parser_input() returned %d", r);
    return r;
}

int parser_connection_close(struct http1_parser_context *context) {
    free(context);
    return 0;
}

long (*connection_get_id)(struct http1_parser_context *);

const char *connection_get_error_message(struct http1_parser_context *context) {
    return context->error_message;
}

