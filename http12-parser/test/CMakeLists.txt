cmake_minimum_required(VERSION 3.0)
project("http-parser-tests")

enable_testing()
include_directories(../src)
link_libraries(httpparser-c)

# Logger test
add_executable(test_logger test_logger.c)
add_test(logger test_logger)

# Parser utility methods test
add_executable(test_http_headers test_http_headers.c)
add_test(http_headers test_http_headers)

# HTTP parser test
add_executable(test_http_parser test_http1_parser.h test_http1_parser.c)
add_test(http_parser test_http_parser)

# Decode test
add_executable(test_decode test_http1_decode.c)
file(COPY data DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_test(decode test_decode)
