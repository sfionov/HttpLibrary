#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include "logger.h"
#include "parser.h"

#include "test_http1_parser.h"

int events_mask;
int content_length;

static void h1_headers(void *attachment, struct http_headers *headers) {
    events_mask |= EVENT_H1_HEADERS;
}

static bool h1_data_started(void *attachment, struct http_headers *headers) {
    events_mask |= EVENT_H1_DATA_STARTED;
    content_length = 0;
    return false;
}

static void h1_data(void *attachment, struct http_headers *headers, const char *data, size_t length) {
    events_mask |= EVENT_H1_DATA;
    content_length += length;
}

static void h1_data_finished(void *attachment, struct http_headers *headers) {
    events_mask |= EVENT_H1_DATA_FINISHED;
}

struct parser_callbacks cbs = {
        .h1_headers = h1_headers,
        .h1_data_started = h1_data_started,
        .h1_data = h1_data,
        .h1_data_finished = h1_data_finished,
};

int main(int argc, char **argv) {
    logger *log = logger_open(NULL, LOG_LEVEL_INFO, NULL, NULL);
    struct http_parser_context *pctx;

    int count = sizeof(messages) / sizeof(struct test_message);
    for (int i = 0; i < count; i++) {
        struct test_message *message = &messages[i];
        fprintf(stderr, "Processing stream:\n%s\n", message->data);
        events_mask = 0;
        content_length = 0;
        assert(http_parser_open(log, HTTP1, message->type, &cbs, NULL, &pctx) == 0);
        assert(pctx != NULL);
        assert(http_parser_input(pctx, message->data, strlen(message->data)) == 0);
        assert(events_mask == message->events_mask);
        assert(content_length == message->content_length);
        assert(http_parser_close(pctx) == 0);
    }

    return 0;
}
