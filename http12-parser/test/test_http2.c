#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include "logger.h"
#include "parser.h"

char HTTP2_PREFACE[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
size_t HTTP2_PREFACE_LEN = sizeof(HTTP2_PREFACE) - 1;
char HTTP2_EMPTY_SETTINGS[] = {
        0x00, 0x00, 0x00, /* length */
        0x04, 0x00, /* settings, no flags */
        0x00, 0x00, 0x00, 0x00 /* stream 0 */
};
size_t HTTP2_EMPTY_SETTINGS_LEN = sizeof(HTTP2_EMPTY_SETTINGS);
char HTTP2_SETTINGS_ACK[] = {
        0x00, 0x00, 0x00, /* length */
        0x04, 0x01, /* settings, no flags */
        0x00, 0x00, 0x00, 0x00 /* stream 0 */
};
size_t HTTP2_SETTINGS_ACK_LEN = sizeof(HTTP2_SETTINGS_ACK);

char *output = NULL;
size_t output_len = 0;

static void reset_output() {
    free(output);
    output = NULL;
    output_len = 0;
}

static void raw_output(void *attachment, const char *data, size_t len) {
    output = realloc(output, output_len + len);
    memcpy(output + output_len, data, len);
    output_len += len;
}

struct parser_callbacks cbs = {
        .raw_output = raw_output
};

int main(int argc, char **argv) {
    logger *log = logger_open(NULL, LOG_LEVEL_TRACE, NULL, NULL);
    struct http_parser_context *pctx;
    int r;

    r = http_parser_open(log, HTTP2, HTTP_SERVER_CONNECTION, &cbs, NULL, &pctx);
    printf("http_parser_open returned %d\n", r);
    assert(r == 0);
    assert(pctx != NULL);

    reset_output();
    r = http_parser_input(pctx, HTTP2_PREFACE, HTTP2_PREFACE_LEN);
    printf("http_parser_input(preface) returned %d\n", r);
    r = http_parser_input(pctx, HTTP2_EMPTY_SETTINGS, HTTP2_EMPTY_SETTINGS_LEN);
    printf("http_parser_input(empty_settings) returned %d\n", r);
    assert(output_len == HTTP2_SETTINGS_ACK_LEN);
    assert(!memcmp(output, HTTP2_SETTINGS_ACK, HTTP2_SETTINGS_ACK_LEN));
    reset_output();
    http_parser_h2_send_settings(pctx);
    assert(output_len == HTTP2_EMPTY_SETTINGS_LEN);
    assert(!memcmp(output, HTTP2_EMPTY_SETTINGS, HTTP2_EMPTY_SETTINGS_LEN));
    r = http_parser_close(pctx);
    printf("http_parser_close returned %d\n", r);

    r = http_parser_open(log, HTTP2, HTTP_CLIENT_CONNECTION, &cbs, NULL, &pctx);
    printf("http_parser_open returned %d\n", r);
    assert(r == 0);
    assert(pctx != NULL);

    reset_output();
    http_parser_h2_send_settings(pctx);
    assert(output_len == HTTP2_EMPTY_SETTINGS_LEN + HTTP2_PREFACE_LEN);
    assert(!memcmp(output, HTTP2_PREFACE, HTTP2_PREFACE_LEN));
    assert(!memcmp(output + HTTP2_PREFACE_LEN, HTTP2_EMPTY_SETTINGS, HTTP2_EMPTY_SETTINGS_LEN));

    r = http_parser_close(pctx);
    printf("http_parser_close returned %d\n", r);
}
