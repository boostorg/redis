# At the moment the official build system is still autotools and this
# file is meant to support Aedis on windows.

# BOOST_ROOT=/opt/boost_1_79/ cmake -DCMAKE_CXX_FLAGS="-g -O0
# -std=c++20 -Wall -Wextra --coverage -fkeep-inline-functions
# -fkeep-static-functions" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
# ~/my/aedis

cmake_minimum_required(VERSION 3.14)

project(
    Aedis
    VERSION 1.3.1
    DESCRIPTION "A redis client designed for performance and scalability"
    HOMEPAGE_URL "https://mzimbres.github.io/aedis"
    LANGUAGES CXX
)

add_library(aedis INTERFACE)
target_include_directories(aedis INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  aedis
  INTERFACE
    Boost::asio
    Boost::assert
    Boost::config
    Boost::core
    Boost::mp11
    Boost::system
    Boost::utility
)

target_compile_features(aedis INTERFACE cxx_std_17)

# Asio bases C++ feature detection on __cplusplus. Make MSVC
# define it correctly
if (MSVC)
    target_compile_options(aedis INTERFACE /Zc:__cplusplus)
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/AedisConfigVersion.cmake"
    COMPATIBILITY AnyNewerVersion
)

find_package(Boost 1.80 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)

enable_testing()
include_directories(include)

# Main function for the examples.
#=======================================================================

add_library(common STATIC
   examples/common/common.cpp
   examples/common/main.cpp
   examples/common/aedis.cpp
)
target_compile_features(common PUBLIC cxx_std_20)

# Executables
#=======================================================================

add_executable(intro examples/intro.cpp)
target_link_libraries(intro common)
target_compile_features(intro PUBLIC cxx_std_20)
add_test(intro intro)
if (MSVC)
    target_compile_options(intro PRIVATE /bigobj)
    target_compile_definitions(intro PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(intro_sync examples/intro_sync.cpp)
target_compile_features(intro_sync PUBLIC cxx_std_20)
add_test(intro_sync intro_sync)
add_test(intro_sync intro_sync)
if (MSVC)
    target_compile_options(intro_sync PRIVATE /bigobj)
    target_compile_definitions(intro_sync PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(chat_room examples/chat_room.cpp)
target_compile_features(chat_room PUBLIC cxx_std_20)
target_link_libraries(chat_room common)
if (MSVC)
    target_compile_options(chat_room PRIVATE /bigobj)
    target_compile_definitions(chat_room PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(containers examples/containers.cpp)
target_compile_features(containers PUBLIC cxx_std_20)
target_link_libraries(containers common)
add_test(containers containers)
if (MSVC)
    target_compile_options(containers PRIVATE /bigobj)
    target_compile_definitions(containers PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(echo_server examples/echo_server.cpp)
target_compile_features(echo_server PUBLIC cxx_std_20)
target_link_libraries(echo_server common)
if (MSVC)
    target_compile_options(echo_server PRIVATE /bigobj)
    target_compile_definitions(echo_server PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(resolve_with_sentinel examples/resolve_with_sentinel.cpp)
target_compile_features(resolve_with_sentinel PUBLIC cxx_std_20)
target_link_libraries(resolve_with_sentinel common)
add_test(resolve_with_sentinel resolve_with_sentinel)
if (MSVC)
    target_compile_options(resolve_with_sentinel PRIVATE /bigobj)
    target_compile_definitions(resolve_with_sentinel PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(serialization examples/serialization.cpp)
target_compile_features(serialization PUBLIC cxx_std_20)
target_link_libraries(serialization common)
add_test(serialization serialization)
if (MSVC)
    target_compile_options(serialization PRIVATE /bigobj)
    target_compile_definitions(serialization PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(subscriber examples/subscriber.cpp)
target_compile_features(subscriber PUBLIC cxx_std_20)
target_link_libraries(subscriber common)
if (MSVC)
    target_compile_options(subscriber PRIVATE /bigobj)
    target_compile_definitions(subscriber PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(intro_tls examples/intro_tls.cpp)
target_compile_features(intro_tls PUBLIC cxx_std_20)
add_test(intro_tls intro_tls)
target_link_libraries(intro_tls OpenSSL::Crypto OpenSSL::SSL)
target_link_libraries(intro_tls common)
if (MSVC)
    target_compile_options(intro_tls PRIVATE /bigobj)
    target_compile_definitions(intro_tls PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(low_level_async examples/low_level_async.cpp)
target_compile_features(low_level_async PUBLIC cxx_std_20)
add_test(low_level_async low_level_async)
target_link_libraries(low_level_async common)
if (MSVC)
    target_compile_options(low_level_async PRIVATE /bigobj)
    target_compile_definitions(low_level_async PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(echo_server_client benchmarks/cpp/asio/echo_server_client.cpp)
target_compile_features(echo_server_client PUBLIC cxx_std_20)
if (MSVC)
    target_compile_options(echo_server_client PRIVATE /bigobj)
    target_compile_definitions(echo_server_client PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(echo_server_direct benchmarks/cpp/asio/echo_server_direct.cpp)
target_compile_features(echo_server_direct PUBLIC cxx_std_20)
if (MSVC)
    target_compile_options(echo_server_direct PRIVATE /bigobj)
    target_compile_definitions(echo_server_direct PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(low_level_sync examples/low_level_sync.cpp)
target_compile_features(low_level_sync PUBLIC cxx_std_17)
add_test(low_level_sync low_level_sync)
if (MSVC)
    target_compile_options(low_level_sync PRIVATE /bigobj)
    target_compile_definitions(low_level_sync PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_exec tests/conn_exec.cpp)
target_compile_features(test_conn_exec PUBLIC cxx_std_20)
add_test(test_conn_exec test_conn_exec)
if (MSVC)
    target_compile_options(test_conn_exec PRIVATE /bigobj)
    target_compile_definitions(test_conn_exec PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_push tests/conn_push.cpp)
target_compile_features(test_conn_push PUBLIC cxx_std_20)
add_test(test_conn_push test_conn_push)
if (MSVC)
    target_compile_options(test_conn_push PRIVATE /bigobj)
    target_compile_definitions(test_conn_push PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_quit tests/conn_quit.cpp)
target_compile_features(test_conn_quit PUBLIC cxx_std_17)
add_test(test_conn_quit test_conn_quit)
if (MSVC)
    target_compile_options(test_conn_quit PRIVATE /bigobj)
    target_compile_definitions(test_conn_quit PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_quit_coalesce tests/conn_quit_coalesce.cpp)
add_test(test_conn_quit_coalesce test_conn_quit_coalesce)
target_compile_features(test_conn_quit_coalesce PUBLIC cxx_std_17)
if (MSVC)
    target_compile_options(test_conn_quit_coalesce PRIVATE /bigobj)
    target_compile_definitions(test_conn_quit_coalesce PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_reconnect tests/conn_reconnect.cpp)
target_compile_features(test_conn_reconnect PUBLIC cxx_std_20)
add_test(test_conn_reconnect test_conn_reconnect)
if (MSVC)
    target_compile_options(test_conn_reconnect PRIVATE /bigobj)
    target_compile_definitions(test_conn_reconnect PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_tls tests/conn_tls.cpp)
add_test(test_conn_tls test_conn_tls)
target_compile_features(test_conn_tls PUBLIC cxx_std_17)
target_link_libraries(test_conn_tls OpenSSL::Crypto OpenSSL::SSL)
if (MSVC)
    target_compile_options(test_conn_tls PRIVATE /bigobj)
    target_compile_definitions(test_conn_tls PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_low_level tests/low_level.cpp)
target_compile_features(test_low_level PUBLIC cxx_std_17)
add_test(test_low_level test_low_level)
if (MSVC)
    target_compile_options(test_low_level PRIVATE /bigobj)
    target_compile_definitions(test_low_level PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_run_cancel tests/conn_run_cancel.cpp)
target_compile_features(test_conn_run_cancel PUBLIC cxx_std_20)
add_test(test_conn_run_cancel test_conn_run_cancel)
if (MSVC)
    target_compile_options(test_conn_run_cancel PRIVATE /bigobj)
    target_compile_definitions(test_conn_run_cancel PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_exec_cancel tests/conn_exec_cancel.cpp)
target_compile_features(test_conn_exec_cancel PUBLIC cxx_std_20)
add_test(test_conn_exec_cancel test_conn_exec_cancel)
if (MSVC)
    target_compile_options(test_conn_exec_cancel PRIVATE /bigobj)
    target_compile_definitions(test_conn_exec_cancel PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_echo_stress tests/conn_echo_stress.cpp)
target_compile_features(test_conn_echo_stress PUBLIC cxx_std_20)
add_test(test_conn_echo_stress test_conn_echo_stress)
if (MSVC)
    target_compile_options(test_conn_echo_stress PRIVATE /bigobj)
    target_compile_definitions(test_conn_echo_stress PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_request tests/request.cpp)
target_compile_features(test_request PUBLIC cxx_std_17)
add_test(test_request test_request)
if (MSVC)
    target_compile_options(test_request PRIVATE /bigobj)
    target_compile_definitions(test_request PRIVATE _WIN32_WINNT=0x0601)
endif()

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

