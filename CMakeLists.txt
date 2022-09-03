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
target_include_directories(aedis INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(aedis
  INTERFACE
    Boost::asio
    Boost::assert
    Boost::config
    Boost::core
    Boost::mp11
    Boost::optional
    Boost::system
    Boost::utility
    Boost::winapi
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/AedisConfigVersion.cmake"
    COMPATIBILITY AnyNewerVersion
)

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
add_executable(subscriber_sentinel examples/subscriber_sentinel.cpp)
add_executable(test_low_level tests/low_level.cpp)
add_executable(test_connection tests/connection.cpp)
add_executable(test_connection_push tests/connection_push.cpp)
add_executable(test_connection_quit tests/connection_quit.cpp)
add_executable(low_level_sync tests/low_level_sync.cpp)

# Tests
#=======================================================================

add_test(containers containers)
add_test(intro intro)
add_test(intro_sync intro_sync)
add_test(serialization serialization)
add_test(test_low_level test_low_level)
add_test(test_connection test_connection)
add_test(test_connection_push test_connection_push)
add_test(test_connection_quit test_connection_quit)
add_test(low_level_sync low_level_sync)

# Install
#=======================================================================

install(TARGETS aedis
    EXPORT aedis
    PUBLIC_HEADER DESTINATION include COMPONENT Development
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
   "${PROJECT_SOURCE_DIR}/cmake/AedisConfig.cmake.in"
   "${PROJECT_BINARY_DIR}/AedisConfig.cmake"
    INSTALL_DESTINATION lib/cmake/aedis
)

install(EXPORT aedis DESTINATION lib/cmake/aedis)
install(FILES "${PROJECT_BINARY_DIR}/AedisConfigVersion.cmake"
    "${PROJECT_BINARY_DIR}/AedisConfig.cmake"
     DESTINATION lib/cmake/aedis)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)

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

# TODO
#=======================================================================

#.PHONY: bench
#bench:
#	pdflatex --jobname=echo-f0 benchmarks/benchmarks.tex
#	pdflatex --jobname=echo-f1 benchmarks/benchmarks.tex
# pdftoppm {input.pdf} {output.file} -png

