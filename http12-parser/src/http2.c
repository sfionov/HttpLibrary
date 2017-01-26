//
// Created by s.fionov on 12.01.17.
//

#include <nghttp2/nghttp2.h>
#include "http2.h"
#include "util.h"

// #define H2P_DEBUG printf("h2p: %s\n", __func__);
// #define LOG_AND_RETURN(m, r) do { printf("h2p: %s\n", m); return r; } while(0)
#define CTX_LOG(args...) logger_log(h12_context->log, args)

static void on_end_headers(const nghttp2_frame *frame, const struct http_parser_context *h12_context,
                           const struct http2_stream_context *stream);

void stream_destroy(struct http2_stream_context *stream) {
    free(stream);
}

int on_begin_frame_callback(nghttp2_session *session,
                            const nghttp2_frame_hd *hd,
                            void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    CTX_LOG(LOG_LEVEL_TRACE, "on_begin_frame_callback(session=%p, h12_context=%p)", session, h12_context);
    h2p_frame_type frame_type;
    int32_t stream_id;
    int r = 0;

//    context->last_frame_type = hd->type;
//    context->last_stream_id = hd->stream_id;

    //context->callbacks->h2_frame(context, last_stream_id, last_frame_type, NULL);

    CTX_LOG(LOG_LEVEL_TRACE, "on_begin_frame_callback() returned %d", r);
    return r;
}

int on_begin_headers_callback(nghttp2_session *session _U_,
                              const nghttp2_frame *frame,
                              void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*)user_data;
    CTX_LOG(LOG_LEVEL_TRACE, "on_begin_headers_callback(session=%p, h12_context=%p)", session, h12_context);
    struct http2_parser_context *context = h12_context->h2;
    struct http2_stream_context *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;
    int r = 0;

    iter = kh_get(h2_streams_ht, context->streams, (khint32_t) frame->hd.stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        CTX_LOG(LOG_LEVEL_ERROR, "ERROR: Stream table corrupted!");
        r = -1;
        goto finish;
    } else {
        stream = kh_value(context->streams, iter);
    }

    if (stream->headers != NULL) {
        printf("ERROR: stream->headers is already allocated");
#if 0
        if (stream->headers-> != 0) {
            free(stream->headers->nva);
        }
        free(stream->headers);
#endif /* 0 */
    }

//    stream->headers = malloc (sizeof(nghttp2_headers));
//    memcpy(stream->headers, frame, sizeof(nghttp2_headers));

    stream->headers->allocated_field_count += frame->headers.nvlen;
    stream->headers->fields = realloc(stream->headers->fields, stream->headers->allocated_field_count *
                                  sizeof(struct http_header_field));

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "on_begin_headers_callback() returned %d", r);
    return r;
}

int on_header_callback(nghttp2_session *session _U_,
                       const nghttp2_frame *frame, const uint8_t *name,
                       size_t namelen, const uint8_t *value,
                       size_t valuelen, uint8_t flags _U_,
                       void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    CTX_LOG(LOG_LEVEL_TRACE, "on_header_callback(session=%p, h12_context=%p, name=%.*s, value=%.*s)", session, h12_context, namelen, name, valuelen, value);
    struct http2_parser_context *context = h12_context->h2;
    struct http2_stream_context *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;
    int r = 0;

    iter = kh_get(h2_streams_ht, context->streams, (khint32_t) frame->hd.stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        CTX_LOG(LOG_LEVEL_ERROR, "ERROR: Stream table corrupted!");
        r = NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        goto finish;
    } else {
        stream = kh_value(context->streams, iter);
    }

    struct http_headers *headers = stream->headers;
    if (stream->headers == NULL) {
        CTX_LOG(LOG_LEVEL_WARN, "WARNING: Memory for header was not allocated!\n");
        r = NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE;
        goto finish;
    }

    if (headers->field_count == headers->allocated_field_count) {
        headers->allocated_field_count++;
        headers->fields = realloc(headers->fields, headers->allocated_field_count * sizeof(struct http_header_field));
    }

    int i = headers->field_count;
    headers->fields[i].name = malloc(namelen + 1);
    memcpy(headers->fields[i].name, name, namelen);
    headers->fields[i].name[namelen + 1] = '\0';

    headers->fields[i].value = malloc(valuelen + 1);
    memcpy(headers->fields[i].value, value, valuelen);
    headers->fields[i].value[valuelen + 1] = '\0';

    headers->field_count++;

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "on_header_callback() returned %d", r);
    return r;
}

ssize_t on_send_callback(nghttp2_session *session,
                         const uint8_t *data, size_t length,
                         int flags, void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    CTX_LOG(LOG_LEVEL_TRACE, "on_send_callback(session=%p, h12_context=%p, data=%p, length=%d, flags=0x%x)", session, h12_context, data, length, flags);
    struct http2_parser_context *context = h12_context->h2;

    if (h12_context->callbacks && h12_context->callbacks->raw_output) {
        h12_context->callbacks->raw_output(h12_context->attachment, (const char *) data, length);
    }

    CTX_LOG(LOG_LEVEL_TRACE, "on_send_callback() returned length %d", length);
    return length;
}

int on_frame_recv_callback(nghttp2_session *session _U_,
                           const nghttp2_frame *frame, void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    CTX_LOG(LOG_LEVEL_TRACE, "on_frame_recv_callback(session=%p, h12_context=%p)", session, h12_context);
    struct http2_parser_context *context = h12_context->h2;
    nghttp2_frame_hd *hd = (nghttp2_frame_hd *)frame;
    struct http2_stream_context *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;
    int r = 0;

    // FIXME: WTF?
#if 0
    if (context->last_stream_id != -1 && context->last_frame_type != -1) {

        context->callbacks->h2_frame(context,
                                     context->last_stream_id,
                                     context->last_frame_type,
                                     frame);

        if (context->last_frame_type != hd->type ||
            context->last_stream_id != hd->stream_id) {

            context->callbacks->h2_frame(context,
                                         hd->stream_id,
                                         hd->type,
                                         frame);
        }
    }

    context->last_stream_id = -1;
    context->last_frame_type = -1;
#endif

    if (frame->hd.type == NGHTTP2_SETTINGS) {
        // Processing SETTINGS frame automatically adds ACK to outbound queue, so flush it.
        nghttp2_session_send(session);
        r = 0;
        goto finish;
    }

    iter = kh_get(h2_streams_ht, context->streams, (khint32_t) frame->hd.stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        CTX_LOG(LOG_LEVEL_ERROR, "ERROR: Stream table corrupted!");
        r = NGHTTP2_ERR_INVALID_STATE;
        goto finish;
    } else {
        stream = kh_value(context->streams, iter);
    }

    /*
     * Documentation says that there is no ..._end_... callbacks, and this callback is fired after
     * all frametype-specific callbacks.
     * So if we need to do some finishing frametype-specific work, it must be done here.
     * Also, all CONTINUATION frames are automatically merged into previous by nghttp2,
     * and we don't need to process them separately.
     */
    switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
            if (frame->headers.hd.flags & NGHTTP2_FLAG_END_HEADERS) {
                on_end_headers(frame, h12_context, stream);
            }
            break;
        default:
            // do nothing
            break;
    }

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "on_frame_recv_callback() returned %d", r);
    return r;
}

static void on_end_headers(const nghttp2_frame *frame, const struct http_parser_context *h12_context, 
                           const struct http2_stream_context *stream) {
    CTX_LOG(LOG_LEVEL_TRACE, "on_end_headers(h12_context=%p)", h12_context);
    if (h12_context->callbacks && h12_context->callbacks->h2_headers) {
        h12_context->callbacks->h2_headers(h12_context->attachment, stream->headers, frame->headers.hd.stream_id);
    }
    CTX_LOG(LOG_LEVEL_TRACE, "on_end_headers() finished");
}

int on_data_chunk_recv_callback(nghttp2_session *session _U_,
                                uint8_t flags _U_, int32_t stream_id,
                                const uint8_t *data, size_t len,
                                void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    CTX_LOG(LOG_LEVEL_TRACE,
            "on_data_chunk_recv_callback(session=%p, h12_context=%p, flags=0x%x, stream_id=%d, len=%d)",
            session, h12_context, flags, stream_id, len);
    struct http2_parser_context *context = h12_context->h2;
    struct http2_stream_context *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;
    int r = 0;

    iter = kh_get(h2_streams_ht, context->streams, stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        CTX_LOG(LOG_LEVEL_ERROR, "Data before headers, this is incorrect");
        r = NGHTTP2_ERR_INVALID_STATE;
        goto finish;
    } else {
        stream = kh_value(context->streams, iter);
        if (stream->id != stream_id) {
            CTX_LOG(LOG_LEVEL_ERROR, "ERROR: Stream table corrupted!");
            r = NGHTTP2_ERR_INVALID_STATE;
            goto finish;
        }

        if (h12_context->callbacks && h12_context->callbacks->h2_data_started) {
            stream->need_decode = h12_context->callbacks->h2_data_started(
                    h12_context->attachment, stream->headers, stream->id);
        }

        if (h12_context->callbacks && h12_context->callbacks->h2_data) {
            h12_context->callbacks->h2_data(h12_context->attachment, stream->headers, stream->id,
                                            (const char *) data, len);
        }
    }

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "on_data_chunk_recv_callback() returned %d", r);
    return r;
}

int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                             uint32_t error_code, void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    CTX_LOG(LOG_LEVEL_TRACE,
            "on_stream_close_callback(session=%p, h12_context=%p, stream_id=%d, error_code=%d)",
            session, h12_context, stream_id, error_code);
    struct http2_parser_context *context = h12_context->h2;
    struct http2_stream_context *stream;
    khiter_t iter;
    int not_found = 0;
    int r = 0;

    iter = kh_get(h2_streams_ht, context->streams, stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        CTX_LOG(LOG_LEVEL_ERROR, "ERROR: Stream table corrupted!");
        r = NGHTTP2_ERR_INVALID_STATE;
        goto finish;
    } else {
        stream = kh_value(context->streams, iter);

        if (stream->id != stream_id) {
            stream_destroy(stream);
            kh_del(h2_streams_ht, context->streams, iter);
            CTX_LOG(LOG_LEVEL_ERROR, "ERROR: Stream table corrupted!");
            r = NGHTTP2_ERR_INVALID_STATE;
            goto finish;
        }

        if (stream->need_decode) {
            // DEFLATE(...)
            ;
        }

        // RST_STREAM
    }

    if (h12_context->callbacks && h12_context->callbacks->h2_data_finished) {
        h12_context->callbacks->h2_data_finished(h12_context->attachment, stream->headers, stream_id, error_code);
    }

    stream_destroy (stream);
    kh_del(h2_streams_ht, context->streams, iter);

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "on_stream_close_callback() returned %d", r);
    return r;
}

#if 1

int on_invalid_header_callback(
        nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
        size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags,
        void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*)user_data;
    int r = 0;

    CTX_LOG(LOG_LEVEL_TRACE, "on_invalid_header_callback(session=%p, h12_context=%p, name=%.*s, value=%.*s)", session, h12_context, namelen, name, valuelen, value);
    CTX_LOG(LOG_LEVEL_TRACE, "on_invalid_header_callback() returned %d", r);
    return r;
}

int on_invalid_frame_recv_callback(nghttp2_session *session,
                                   const nghttp2_frame *frame,
                                   int lib_error_code,
                                   void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*)user_data;
    int r = 0;

    CTX_LOG(LOG_LEVEL_TRACE, "on_invalid_frame_recv_callback(session=%p, h12_context=%p, error_code=%d)", session, h12_context, lib_error_code);

    //context->callbacks->h2_error(context, H2P_ERROR_INVALID_FRAME, nghttp2_error_code);
    CTX_LOG(LOG_LEVEL_TRACE, "on_invalid_frame_recv_callback() returned %d", r);
    return r;
}

#endif

int error_callback(nghttp2_session *session, const char *msg,
                   size_t len, void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*)user_data;
    int r = 0;

    CTX_LOG(LOG_LEVEL_TRACE, "error_callback(session=%p, h12_context=%p, msg=\"%.*s\")", session, h12_context, len, msg);

    // context->callbacks->h2_error(context, H2P_ERROR_MESSAGE, msg);
    CTX_LOG(LOG_LEVEL_TRACE, "error_callback() returned %d", r);
    return r;
}


int http2_parser_init(struct http_parser_context *h12_context) {
    logger_log(h12_context->log, LOG_LEVEL_TRACE, "http2_parser_init(h12_context=%p)", h12_context);
    int                         status = 0;
    nghttp2_session             *ngh2_session;
    nghttp2_session_callbacks   *ngh2_callbacks;

    status = nghttp2_session_callbacks_new(&ngh2_callbacks);

    // recv
    //nghttp2_session_callbacks_set_recv_callback(ngh2_callbacks, recv_callback);

    // begin frame
    nghttp2_session_callbacks_set_on_begin_frame_callback(ngh2_callbacks,
                                                          on_begin_frame_callback);

    // frame recv (end frame)
    nghttp2_session_callbacks_set_on_frame_recv_callback(ngh2_callbacks,
                                                         on_frame_recv_callback);

    // invalid frame recv
#if 0
    nghttp2_session_callbacks_set_on_invalid_frame_recv_callback (
    ngh2_callbacks, on_invalid_frame_recv_callback);
#endif

    // data chunck recv
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
            ngh2_callbacks, on_data_chunk_recv_callback);

    // stream close
    nghttp2_session_callbacks_set_on_stream_close_callback(
            ngh2_callbacks, on_stream_close_callback);

    // header
    nghttp2_session_callbacks_set_on_header_callback(ngh2_callbacks,
                                                     on_header_callback);

    // begin headers
    nghttp2_session_callbacks_set_on_begin_headers_callback(ngh2_callbacks,
                                                            on_begin_headers_callback);

    // error
    nghttp2_session_callbacks_set_error_callback(ngh2_callbacks, error_callback);

    // output callback
    nghttp2_session_callbacks_set_send_callback(ngh2_callbacks, on_send_callback);

    switch (h12_context->type) {
        case HTTP_SERVER_CONNECTION:
            status = nghttp2_session_server_new(&ngh2_session, ngh2_callbacks, h12_context);
            break;
        case HTTP_CLIENT_CONNECTION:
            status = nghttp2_session_client_new(&ngh2_session, ngh2_callbacks, h12_context);
            break;
        default:
            // this should not happen
            ngh2_session = NULL;
            status = -1;
            break;
    }
    nghttp2_session_callbacks_del(ngh2_callbacks);

    if (status != 0) {
        fprintf(stderr, "nghttp2 returned non-zero status: %d\n", status);
        goto finish;
    }

    h12_context->h2 = malloc(sizeof(struct http2_parser_context));
    memset(h12_context->h2, 0, sizeof(struct http2_parser_context));

    h12_context->h2->session = ngh2_session;

    h12_context->h2->streams = kh_init(h2_streams_ht);

    finish:
    logger_log(h12_context->log, LOG_LEVEL_TRACE, "http2_parser_init() returned %d\n", status);
    return status;
}


int http2_parser_input(struct http_parser_context *h12_context, const char *data, size_t len) {
    ssize_t nbytes;
    int r = 0;

    if (h12_context == NULL || data == NULL || len == 0) return -1;
    logger_log(h12_context->log, LOG_LEVEL_TRACE, "http2_parser_input(h12_context=%p, len=%d)", h12_context, (int) len);

    if (nghttp2_session_want_read(h12_context->h2->session) == 0) {
        logger_log(h12_context->log, LOG_LEVEL_WARN, "nghttp2_session_want_read == 0\n");
        r = -1;
        goto finish;
    }

    nbytes = nghttp2_session_mem_recv(h12_context->h2->session, (const uint8_t *) data, len);
    //s = nghttp2_session_send (connection->ngh2_session);

    if (nbytes < 0) printf("ERROR: %s.\n", nghttp2_strerror((int) nbytes));

    finish:
    logger_log(h12_context->log, LOG_LEVEL_TRACE, "http2_parser_input() returned %d\n", r);
    return r;
}


int http2_parser_close(struct http_parser_context *h12_context)  {
    CTX_LOG(LOG_LEVEL_TRACE, "http2_parser_close(h12_context=%p)", h12_context);
    struct http2_parser_context *context = h12_context->h2;
    khiter_t iter;
    int r = 0;

    if (context == NULL) return -1;

    for (iter = kh_begin(context->streams); iter != kh_end(context->streams); ++iter) {
        if (kh_exist(context->streams, iter)) {
            stream_destroy (kh_value(context->streams, iter));
            // kh_del(h2_streams_ht, context->streams, iter);
        }
    }

    kh_destroy(h2_streams_ht, context->streams);
    nghttp2_session_del(context->session);
#if 0
    if (context->headers != NULL) {
    if (context->headers->nvlen != 0) {
      free(context->headers->nva);
    }
    free(context->headers);
  }
#endif
    free(context);
    h12_context->h2 = NULL;

    CTX_LOG(LOG_LEVEL_TRACE, "http2_parser_close() returned %d\n", r);
    return r;
}

static const char HTTP2_EMPTY_SETTINGS[] = {
        0x00, 0x00, 0x00, /* length */
        0x04, 0x00, /* settings, no flags */
        0x00, 0x00, 0x00, 0x00 /* stream 0 */
};
static const size_t HTTP2_EMPTY_SETTINGS_LEN = sizeof(HTTP2_EMPTY_SETTINGS);

int http_parser_h2_send_settings(struct http_parser_context *h12_context) {
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_h2_send_settings(h12_context=%p)", h12_context);
    nghttp2_session *session = h12_context->h2->session;
    int r = nghttp2_submit_settings(session, 0, NULL, 0);
    if (r != 0) {
        goto finish;
    }
    r = nghttp2_session_send(session);

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_h2_send_settings() returned %d\n", r);
    return r;
}

int http_parser_h2_send_headers(struct http_parser_context *h12_context, int32_t stream_id, struct http_headers *headers) {
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_h2_send_headers(h12_context=%p, stream_id=%d)", h12_context, stream_id);
    int r = 0;
    nghttp2_nv *nva = malloc(sizeof(nghttp2_nv) * headers->field_count);

    memset(nva, 0, sizeof(nghttp2_nv) * headers->field_count);
    for (int i = 0; i < headers->field_count; i++) {
        nva[i].name = (uint8_t *) headers->fields[i].name;
        nva[i].namelen = headers->fields[i].name != NULL ? strlen(headers->fields[i].name) : 0;
        nva[i].value = (uint8_t *) headers->fields[i].value;
        nva[i].valuelen = headers->fields[i].value != NULL ? strlen(headers->fields[i].value) : 0;
    }

    nghttp2_session *session = h12_context->h2->session;
    r = nghttp2_submit_headers(session, NGHTTP2_FLAG_END_HEADERS, stream_id, NULL, nva, (size_t) headers->field_count, NULL);
    if (r != 0) {
        goto finish;
    }
    r = nghttp2_session_send(session);

    finish:
    CTX_LOG(LOG_LEVEL_TRACE, "http_parser_h2_send_headers() returned %d\n", r);
    return r;
}

int http_parser_h2_send_data(struct http_parser_context *h12_context, int32_t stream_id, const char *data, size_t len, bool eof) {

}

int http_parser_h2_reset_stream(struct http_parser_context *context, int32_t stream_id, int error_code) {

}

int http_parser_h2_goaway(struct http_parser_context *context, int error_code) {

}
