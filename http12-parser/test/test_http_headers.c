//
// Parser utility methods test
// Created by s.fionov on 01.12.16.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>

#include "http_header.h"
#include "util.h"

#define NONEMPTY_FIELD_NAME "Non-Empty-Field"
#define NONEXISTING_FIELD_NAME "Non-Existing-Field"
#define EMPTY_FIELD_NAME  "Empty-Field"

static const char correct_output[] = "GET / HTTP/1.1\r\n"
        "Non-Empty-Field: 1\r\n"
        "Empty-Field2: \r\n"
        "Non-Empty-Field2: 2\r\n\r\n";

static const char correct_output_response[] = "HTTP/1.1 200 OK\r\n"
        "Non-Empty-Field: 1\r\n"
        "Empty-Field2: \r\n"
        "Non-Empty-Field2: 2\r\n\r\n";

static char *const HTTP_STATUS_OK = "OK";

int main() {
    struct http_headers *message;
    http_headers_create(&message);
    assert (message != NULL);

    // Cloning empty message
    struct http_headers *clone = http_headers_clone(message);
    assert (clone != NULL);
    http_headers_free(clone);

    http_headers_set_method(message, "GET");
    http_headers_set_path(message, "/");
    // Add header field named "Empty-Field"
    http_headers_put_field(message, EMPTY_FIELD_NAME "-blablabla", NULL);
    http_headers_put_field(message, NONEMPTY_FIELD_NAME, NULL);
    http_headers_put_field(message, NONEMPTY_FIELD_NAME, "1");
    size_t value_len = INT_MAX;
    const char *field_value = http_headers_get_field(message, EMPTY_FIELD_NAME);
    assert (field_value != NULL);
    // value len is 0 and field_value is empty string
    assert (strlen(field_value) == 0);
    assert (strcmp(field_value, "") == 0);
    field_value = http_headers_get_field(message, NONEMPTY_FIELD_NAME);
    // value len is 1 and field_value is "1"
    assert (strlen(field_value) == 1);
    assert (strcmp(field_value, "1") == 0);
    field_value = http_headers_get_field(message, NONEXISTING_FIELD_NAME);
    assert (field_value == NULL);

    int r = http_headers_remove_field(message, "empty-field" /* lowercase */);
    assert (r == 0);
    r = http_headers_remove_field(message, "empty-field" /* lowercase */);
    // No such field error
    assert (r == 1);

    http_headers_put_field(message, EMPTY_FIELD_NAME "2", NULL);
    http_headers_put_field(message, EMPTY_FIELD_NAME "2", "");
    http_headers_put_field(message, NONEMPTY_FIELD_NAME "2", "2");

    size_t output_len;
    char *output = http_headers_to_http1_message(message, &output_len);
    assert (output != NULL);
    assert (!strcmp(output, correct_output));
    free(output);

    clone = http_headers_clone(message);

    http_headers_set_status(clone, 200);
    http_headers_set_status_string(clone, HTTP_STATUS_OK);

    output = http_headers_to_http1_message(clone, &output_len);
    assert (output != NULL);
    assert (!strcmp(output, correct_output_response));
    free(output);
}

