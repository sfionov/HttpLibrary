#define _GNU_SOURCE

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <stdbool.h>

#include "logger.h"
#include "parser.h"

#define DECODE true

struct process_context {
    char *buf;
    size_t pos;
    int finished;
} process_context;

struct test_file {
    char *name;
    char *contents;
    size_t size;
};

static void h1_headers(void *attachment, struct http_headers *headers) {

}

static bool h1_data_started(void *attachment, struct http_headers *headers) {
    fputc('[', stderr);
    return DECODE;
}

static void h1_data(void *attachment, struct http_headers *headers, const char *data, size_t length) {
    fputc('.', stderr);
    memcpy(process_context.buf + process_context.pos, data, length);
    process_context.pos += length;
}

static void h1_data_finished(void *attachment, struct http_headers *headers) {
    fputc(']', stderr);
    process_context.finished = 1;
}

struct parser_callbacks cbs = {
        .h1_headers = h1_headers,
        .h1_data_started = h1_data_started,
        .h1_data = h1_data,
        .h1_data_finished = h1_data_finished,
};

struct test_file license_txt;
struct test_file license_txt_http_gzip;
struct test_file license_txt_http_gzip_chunked;

void prepare(char *file_name, struct test_file *test_file) {
    fprintf(stderr, "Reading %s... ", file_name);
    FILE *file = fopen(file_name, "r");
    assert (file != NULL);
    fseek(file, 0L, SEEK_END);
    test_file->size = (size_t) ftell(file);
    fseek(file, 0L, SEEK_SET);
    test_file->contents = malloc(test_file->size);
    char *pos = test_file->contents;
    size_t r;
    while ((r = fread(pos, 1, 4096, file)) > 0) {
        pos += r;
    }
    size_t total_read = pos - test_file->contents;
    fprintf(stderr, "%ld bytes read.\n", total_read);
    assert (total_read == test_file->size);
    asprintf(&test_file->name, "%s", file_name);
}

void process(struct http_parser_context *cctx, struct test_file *file, struct test_file *uncompressed_file) {
    fprintf(stderr, "Processing %s: ", file->name);
    process_context.buf = malloc(uncompressed_file->size);
    process_context.pos = 0;
    process_context.finished = 0;
    int r;
    r = http_parser_input(cctx, file->contents, file->size);
    fputc('\n', stderr);
    if (r != 0) {
        fprintf(stderr, "parser_input() returned non-zero status: %d\n", r);
        exit(1);
    }
    // Input is fully processed
    assert (process_context.finished);
    // Length is same
    assert (process_context.pos == uncompressed_file->size);
    // Contents match
    assert (!memcmp(process_context.buf, uncompressed_file->contents, uncompressed_file->size));
}

int main(int argc, char **argv) {
    prepare("data/LICENSE-2.0.txt", &license_txt);
    prepare("data/LICENSE-2.0.txt-HTTP-gzip.bin", &license_txt_http_gzip);
    prepare("data/LICENSE-2.0.txt-HTTP-gzip-chunked.bin", &license_txt_http_gzip_chunked);

    logger *log = logger_open(NULL, LOG_LEVEL_TRACE, NULL, NULL);
    struct http_parser_context *pctx;
    if (http_parser_open(log, HTTP1, HTTP_INCOMING, &cbs, NULL, &pctx) != 0) {
        fprintf(stderr, "Error creating parser\n");
        return 1;
    }

    process(pctx, &license_txt_http_gzip, &license_txt);
    process(pctx, &license_txt_http_gzip_chunked, &license_txt);
    return 0;
}
