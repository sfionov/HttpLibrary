//
// Created by s.fionov on 12.01.17.
//

#include "http2.h"

#define H2P_DEBUG printf("h2p: %s\n", __func__);
#define LOG_AND_RETURN(m, r) do { printf("h2p: %s\n", m); return r; } while(0)

void stream_destroy(h2p_stream *stream) {
    free(stream);
}

int on_begin_frame_callback(nghttp2_session *session,
                            const nghttp2_frame_hd *hd,
                            void *user_data) {
    struct http_parser_context *context = (struct http_parser_context*) user_data;
    h2p_frame_type frame_type;
    int32_t stream_id;

    H2P_DEBUG

//    context->last_frame_type = hd->type;
//    context->last_stream_id = hd->stream_id;

    //context->callbacks->h2_frame(context, last_stream_id, last_frame_type, NULL);

    return 0;
}

int on_begin_headers_callback(nghttp2_session *session _U_,
                              const nghttp2_frame *frame,
                              void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*)user_data;
    struct http2_parser_context *context = h12_context->h2;
    h2p_stream *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;

    H2P_DEBUG

    iter = kh_get(h2_streams_ht, context->streams, (khint32_t) frame->hd.stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        LOG_AND_RETURN("ERROR: Stream table corrupted!\n", -1);
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

    return 0;
}

int on_header_callback(nghttp2_session *session _U_,
                       const nghttp2_frame *frame, const uint8_t *name,
                       size_t namelen, const uint8_t *value,
                       size_t valuelen, uint8_t flags _U_,
                       void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    struct http2_parser_context *context = h12_context->h2;
    h2p_stream *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;

    H2P_DEBUG

    iter = kh_get(h2_streams_ht, context->streams, (khint32_t) frame->hd.stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        LOG_AND_RETURN("ERROR: Stream table corrupted!\n", -1);
    } else {
        stream = kh_value(context->streams, iter);
    }

    struct http_headers *headers = stream->headers;
    if (stream->headers == NULL) {
        H2P_DEBUG("WARNING: Memory for header was not allocated!\n");
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

    return 0;
}

int on_frame_recv_callback(nghttp2_session *session _U_,
                           const nghttp2_frame *frame, void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    struct http2_parser_context *context = h12_context->h2;
    nghttp2_frame_hd *hd = (nghttp2_frame_hd *)frame;
    h2p_stream *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;

    H2P_DEBUG

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

    iter = kh_get(h2_streams_ht, context->streams, (khint32_t) frame->hd.stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        LOG_AND_RETURN("ERROR: Stream table corrupted!\n", -1);
    } else {
        stream = kh_value(context->streams, iter);
    }

    switch (frame->hd.type) {
        case NGHTTP2_HEADERS:
            if (frame->headers.hd.flags & NGHTTP2_FLAG_END_HEADERS) {
                h12_context->callbacks->h2_headers(stream->headers, frame->headers.hd.stream_id);
            }
            break;
        default:
            // do nothing
            break;
    }

    return 0;
}

int on_data_chunk_recv_callback(nghttp2_session *session _U_,
                                uint8_t flags _U_, int32_t stream_id,
                                const uint8_t *data, size_t len,
                                void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    struct http2_parser_context *context = h12_context->h2;
    h2p_stream *stream;
    khiter_t iter;
    int not_found = 0, push_ret = 0;

    H2P_DEBUG

    iter = kh_get(h2_streams_ht, context->streams, stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        LOG_AND_RETURN("Data before headers, this is incorrect\n", -1);
        stream = malloc (sizeof(h2p_stream));
        iter = kh_put(h2_streams_ht, context->streams, stream_id, &push_ret);
        kh_value(context->streams, iter) = stream;

        stream->id = (uint32_t) stream_id;

        stream->need_decode = h12_context->callbacks->h2_data_started(stream->headers, stream->id);

        h12_context->callbacks->h2_data(stream->headers, stream->id, (const char *) data, len);

    } else {
        stream = kh_value(context->streams, iter);
        if (stream->id != stream_id) {
            LOG_AND_RETURN("ERROR: Stream table corrupted!\n", -1);
        }

        stream->need_decode = h12_context->callbacks->h2_data_started(stream->headers, stream->id);
        h12_context->callbacks->h2_data(stream->headers, stream->id, (const char *) data, len);
    }

    return 0;
}

int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                             uint32_t error_code, void *user_data) {
    struct http_parser_context *h12_context = (struct http_parser_context*) user_data;
    struct http2_parser_context *context = h12_context->h2;
    h2p_stream *stream;
    khiter_t iter;
    int not_found = 0;

    H2P_DEBUG

    iter = kh_get(h2_streams_ht, context->streams, stream_id);
    not_found = (iter == kh_end(context->streams));

    if (not_found) {
        LOG_AND_RETURN("ERROR: Stream table corrupted!\n", -1);
    } else {
        stream = kh_value(context->streams, iter);

        if (stream->id != stream_id) {
            stream_destroy(stream);
            kh_del(h2_streams_ht, context->streams, iter);
            LOG_AND_RETURN("ERROR: Stream table corrupted!\n", -1);
        }

        if (stream->need_decode) {
            // DEFLATE(...)
            ;
        }

        // RST_STREAM
    }

    h12_context->callbacks->h2_data_finished(context, stream_id, error_code);

    stream_destroy (stream);
    kh_del(h2_streams_ht, context->streams, iter);
    return 0;
}

#if 1

int on_invalid_header_callback(
        nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
        size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags,
        void *user_data) {
    struct http_parser_context *context = (struct http_parser_context*)user_data;

    H2P_DEBUG
    return 0;
}

int on_invalid_frame_recv_callback(nghttp2_session *session,
                                   const nghttp2_frame *frame,
                                   int lib_error_code,
                                   void *user_data) {
    struct http_parser_context *context = (struct http_parser_context*)user_data;

    H2P_DEBUG

    //context->callbacks->h2_error(context, H2P_ERROR_INVALID_FRAME, nghttp2_error_code);
    return 0;
}

#endif

int error_callback(nghttp2_session *session, const char *msg,
                   size_t len, void *user_data) {
    struct http_parser_context *context = (struct http_parser_context*)user_data;

    H2P_DEBUG;

    fprintf(stderr, "ERROR: %s:%d: %s", __func__, __LINE__, msg);
    // context->callbacks->h2_error(context, H2P_ERROR_MESSAGE, msg);
    return 0;
}


int http2_parser_init(struct http_parser_context *h12_context) {
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

    switch (h12_context->type) {
        case HTTP_INCOMING:
            status = nghttp2_session_server_new(&ngh2_session, ngh2_callbacks, h12_context);
            break;
        case HTTP_OUTGOING:
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
        return status;
    }

    h12_context->h2->session = ngh2_session;

    h12_context->h2->streams = kh_init(h2_streams_ht);

    return 0;
}


int http2_parser_input(struct http_parser_context *h12_context, const char *data, size_t len) {
    ssize_t nbytes;

    if (h12_context == NULL || data == NULL || len == 0) return -1;

    if (nghttp2_session_want_read(h12_context->h2->session) == 0) {
        LOG_AND_RETURN("nghttp2_session_want_read = 0", -1);
    }

    nbytes = nghttp2_session_mem_recv(h12_context->h2->session, (const uint8_t *) data, len);
    //s = nghttp2_session_send (connection->ngh2_session);

    if (nbytes < 0) printf("ERROR: %s.\n", nghttp2_strerror((int) nbytes));

    return 0;
}


int http2_parser_close(struct http_parser_context *h12_context)  {
    struct http2_parser_context *context = h12_context->h2;
    khiter_t iter;

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

    return 0;
}
