# At the moment the official build system is still autotools and this
# file is meant to support Aedis on windows.

# BOOST_ROOT=/opt/boost_1_79_0/ cmake -DCMAKE_CXX_FLAGS="-g -O0
# -std=c++20 -Wall -Wextra --coverage -fkeep-inline-functions
# -fkeep-static-functions" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
# ~/my/aedis

cmake_minimum_required(VERSION 3.14)

project(
    Aedis
    VERSION 1.0.0
    DESCRIPTION "An async redis client designed for performance and scalability"
    HOMEPAGE_URL "https://mzimbres.github.io/aedis"
    LANGUAGES CXX
)

add_library(aedis INTERFACE)
target_include_directories(aedis INTERFACE include)

find_package(Boost 1.79 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

enable_testing()
include_directories(include)

# Executables
#=======================================================================

add_executable(chat_room examples/chat_room.cpp)
add_executable(containers examples/containers.cpp)
add_executable(echo_server examples/echo_server.cpp)
add_executable(intro examples/intro.cpp)
add_executable(intro_sync examples/intro_sync.cpp)
add_executable(serialization examples/serialization.cpp)
add_executable(subscriber examples/subscriber.cpp)
add_executable(subscriber_sync examples/subscriber_sync.cpp)
add_executable(test_low_level tests/low_level.cpp)
add_executable(test_connection tests/connection.cpp)
add_executable(test_connection_push tests/connection_push.cpp)
add_executable(low_level_sync tests/low_level_sync.cpp)

# Tests
#=======================================================================

add_test(containers containers)
add_test(intro intro)
add_test(intro_sync intro_sync)
add_test(serialization serialization)
add_test(test_low_level test_low_level)
add_test(test_connection_push test_connection_push)
add_test(low_level_sync low_level_sync)

# Install
#=======================================================================

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/boost
   DESTINATION include
   FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

# Doxygen
#=======================================================================

set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/doc")

configure_file(doc/Doxyfile.in doc/Doxyfile @ONLY)

add_custom_target(
    doc
    COMMAND doxygen "${PROJECT_BINARY_DIR}/doc/Doxyfile"
    COMMENT "Building documentation using Doxygen"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    VERBATIM
)

# Coverage
#=======================================================================

set(
    COVERAGE_TRACE_COMMAND
    lcov --capture
    -output-file "${PROJECT_BINARY_DIR}/coverage.info"
    --directory "${PROJECT_BINARY_DIR}"
    --include "${PROJECT_SOURCE_DIR}/include/*"
)

set(
    COVERAGE_HTML_COMMAND
    genhtml --legend -f -q
    "${PROJECT_BINARY_DIR}/coverage.info"
    --prefix "${PROJECT_SOURCE_DIR}"
    --output-directory "${PROJECT_BINARY_DIR}/coverage_html"
)

add_custom_target(
    coverage
    COMMAND ${COVERAGE_TRACE_COMMAND}
    COMMAND ${COVERAGE_HTML_COMMAND}
    COMMENT "Generating coverage report"
    VERBATIM
)

# Distribution
#=======================================================================

include(CPack)

