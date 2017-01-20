cmake_minimum_required(VERSION 3.0)
project(nghttp2 VERSION 1.13.0)

# See versioning rule:
#  http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
set(LT_CURRENT  23)
set(LT_REVISION 0)
set(LT_AGE      9)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(Version)

math(EXPR LT_SOVERSION "${LT_CURRENT} - ${LT_AGE}")
set(LT_VERSION "${LT_SOVERSION}.${LT_AGE}.${LT_REVISION}")
set(PACKAGE_VERSION     "${PROJECT_VERSION}")
HexVersion(PACKAGE_VERSION_NUM ${PROJECT_VERSION_MAJOR} ${PROJECT_VERSION_MINOR} ${PROJECT_VERSION_PATCH})

if(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(HINT_UNUSED_PARAM   "__attribute__((unused))")
  set(HINT_NORETURN       "__attribute__((noreturn))")
else()
  set(HINT_UNUSED_PARAM)
  set(HINT_NORETURN)
endif()

add_definitions(-DHAVE_CONFIG_H)
configure_file(cmakeconfig.h.in config.h)
include_directories(
  "${PROJECT_BINARY_DIR}" # for config.h
)

add_subdirectory(lib)
