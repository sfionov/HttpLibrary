cmake_minimum_required(VERSION 3.0)
project(http-parser)

find_package(Threads)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

set(SOURCE_FILES
        third-party/http-parser/http_parser.h third-party/http-parser/http_parser.c
        src/logger.h src/logger.c
        third-party/klib/khash.h
        src/http_header.h src/http_header.c
        src/http1.h src/http1.c
        src/http2.h src/http2.c
        src/util.h src/util.c
        src/parser.h src/parser.c
        src/h2p.h src/h2p.c)

include_directories(third-party/http-parser)
include_directories(third-party/klib)
include_directories(third-party/nghttp2/lib/includes)

link_libraries(z pthread nghttp2)
add_library(httpparser-c ${SOURCE_FILES})
set_property(TARGET httpparser-c PROPERTY POSITION_INDEPENDENT_CODE ON)

# Tests

enable_testing()
# add_subdirectory(test)

add_subdirectory(third-party/nghttp2)

