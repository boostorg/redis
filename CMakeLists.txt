# At the moment the official build system is still autotools and this
# file is meant to support Aedis on windows.

# BOOST_ROOT=/opt/boost_1_79/ cmake -DCMAKE_CXX_FLAGS="-g -O0
# -std=c++20 -Wall -Wextra --coverage -fkeep-inline-functions
# -fkeep-static-functions" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
# ~/my/aedis

cmake_minimum_required(VERSION 3.14)

project(
    Aedis
    VERSION 1.1.1
    DESCRIPTION "A redis client designed for performance and scalability"
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

target_compile_features(aedis INTERFACE cxx_std_17)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/AedisConfigVersion.cmake"
    COMPATIBILITY AnyNewerVersion
)

find_package(Boost 1.79 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)

enable_testing()
include_directories(include)

# Executables
#=======================================================================

#add_executable(intro_sync examples/intro_sync.cpp) // Uncomment after update to Boost 1.80
add_executable(chat_room examples/chat_room.cpp)
add_executable(containers examples/containers.cpp)
add_executable(echo_server examples/echo_server.cpp)
add_executable(echo_server_client benchmarks/cpp/asio/echo_server_client.cpp)
add_executable(echo_server_direct benchmarks/cpp/asio/echo_server_direct.cpp)
add_executable(intro examples/intro.cpp)
add_executable(intro_tls examples/intro_tls.cpp)
add_executable(low_level_sync examples/low_level_sync.cpp)
add_executable(serialization examples/serialization.cpp)
add_executable(subscriber examples/subscriber.cpp)
add_executable(subscriber_sentinel examples/subscriber_sentinel.cpp)
add_executable(test_conn_connect tests/conn_connect.cpp)
add_executable(test_conn_request tests/conn_request.cpp)
add_executable(test_conn_push tests/conn_push.cpp)
add_executable(test_conn_quit tests/conn_quit.cpp)
add_executable(test_conn_quit_coalesce tests/conn_quit_coalesce.cpp)
add_executable(test_conn_reconnect tests/conn_reconnect.cpp)
add_executable(test_conn_tls tests/conn_tls.cpp)
add_executable(test_low_level tests/low_level.cpp)
add_executable(test_conn_cancel_run tests/conn_cancel_run.cpp)
add_executable(test_conn_cancel_exec tests/conn_cancel_exec.cpp)

target_compile_features(chat_room PUBLIC cxx_std_20)
target_compile_features(containers PUBLIC cxx_std_20)
target_compile_features(echo_server PUBLIC cxx_std_20)
target_compile_features(echo_server_client PUBLIC cxx_std_20)
target_compile_features(echo_server_direct PUBLIC cxx_std_20)
target_compile_features(intro PUBLIC cxx_std_17)
target_compile_features(intro_tls PUBLIC cxx_std_17)
target_compile_features(low_level_sync PUBLIC cxx_std_17)
target_compile_features(serialization PUBLIC cxx_std_17)
target_compile_features(subscriber PUBLIC cxx_std_20)
target_compile_features(subscriber_sentinel PUBLIC cxx_std_20)
target_compile_features(test_conn_connect PUBLIC cxx_std_17)
target_compile_features(test_conn_request PUBLIC cxx_std_20)
target_compile_features(test_conn_push PUBLIC cxx_std_20)
target_compile_features(test_conn_quit PUBLIC cxx_std_17)
target_compile_features(test_conn_quit_coalesce PUBLIC cxx_std_17)
target_compile_features(test_conn_reconnect PUBLIC cxx_std_20)
target_compile_features(test_conn_tls PUBLIC cxx_std_17)
target_compile_features(test_low_level PUBLIC cxx_std_17)
target_compile_features(test_conn_cancel_run PUBLIC cxx_std_20)
target_compile_features(test_conn_cancel_exec PUBLIC cxx_std_20)

target_link_libraries(intro_tls OpenSSL::Crypto OpenSSL::SSL)
target_link_libraries(test_conn_tls OpenSSL::Crypto OpenSSL::SSL)

# Tests
#=======================================================================

add_test(containers containers)
add_test(intro intro)
add_test(intro_tls intro_tls)
#add_test(intro_sync intro_sync)
add_test(serialization serialization)
add_test(low_level_sync low_level_sync)
add_test(test_low_level test_low_level)
add_test(test_conn_request test_conn_request)
add_test(test_conn_connect test_conn_connect)
add_test(test_conn_push test_conn_push)
add_test(test_conn_quit test_conn_quit)
add_test(test_conn_quit_coalesce test_conn_quit_coalesce)
add_test(test_conn_reconnect test_conn_reconnect)
add_test(test_conn_tls test_conn_tls)
add_test(test_conn_cancel_run test_conn_cancel_run)
add_test(test_conn_cancel_exec test_conn_cancel_exec)

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

