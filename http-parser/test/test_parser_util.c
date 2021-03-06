//
// Parser utility methods test
// Created by s.fionov on 01.12.16.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>

#include "parser.h"

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
    http_message *message = http_message_create();
    assert (message != NULL);

    // Cloning empty message
    http_message *clone = http_message_clone(message);
    assert (clone != NULL);
    http_message_free(clone);

    http_message_set_method(message, "GET", strlen("GET"));
    http_message_set_url(message, "/", 1);
    // Add header field named "Empty-Field"
    http_message_add_header_field(message, EMPTY_FIELD_NAME "-blablabla", 11);
    http_message_add_header_field(message, NONEMPTY_FIELD_NAME, strlen(NONEMPTY_FIELD_NAME));
    http_message_set_header_field(message, NONEMPTY_FIELD_NAME, strlen(NONEMPTY_FIELD_NAME) + 1 /* plus null byte */, "1", 1);
    size_t value_len = INT_MAX;
    const char *field_value = http_message_get_header_field(message, EMPTY_FIELD_NAME, 11, &value_len);
    assert (field_value != NULL);
    // value len is 0 and field_value is empty string
    assert (value_len == 0);
    assert (strcmp(field_value, "") == 0);
    field_value = http_message_get_header_field(message, NONEMPTY_FIELD_NAME, strlen(NONEMPTY_FIELD_NAME), &value_len);
    // value len is 1 and field_value is "1"
    assert (value_len == 1);
    assert (strcmp(field_value, "1") == 0);
    field_value = http_message_get_header_field(message, NONEXISTING_FIELD_NAME, 11, &value_len);
    assert (field_value == NULL);

    int r = http_message_del_header_field(message, "empty-field" /* lowercase */, 11);
    assert (r == 0);
    r = http_message_del_header_field(message, "empty-field" /* lowercase */, 11);
    // No such field error
    assert (r == 1);

    http_message_add_header_field(message, EMPTY_FIELD_NAME "2", strlen(EMPTY_FIELD_NAME "2"));
    http_message_set_header_field(message, EMPTY_FIELD_NAME "2", strlen(EMPTY_FIELD_NAME "2"), "", 0);
    http_message_add_header_field(message, NONEMPTY_FIELD_NAME "2", strlen(NONEMPTY_FIELD_NAME "2"));
    http_message_set_header_field(message, NONEMPTY_FIELD_NAME "2", strlen(NONEMPTY_FIELD_NAME "2"), "2", 1);

    size_t output_len;
    char *output = http_message_raw(message, &output_len);
    assert (output != NULL);
    assert (!strcmp(output, correct_output));
    free(output);

    clone = http_message_clone(message);

    http_message_set_status_code(clone, 200);
    http_message_set_status(clone, HTTP_STATUS_OK, strlen(HTTP_STATUS_OK));

    output = http_message_raw(clone, &output_len);
    assert (output != NULL);
    assert (!strcmp(output, correct_output_response));
    free(output);
}

