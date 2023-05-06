cmake_minimum_required(VERSION 3.14)

project(
    boost_redis
    VERSION 1.4.1
    DESCRIPTION "A redis client library"
    HOMEPAGE_URL "https://boostorg.github.io/redis/"
    LANGUAGES CXX
)

add_library(boost_redis INTERFACE)
add_library(Boost::redis ALIAS boost_redis)
target_include_directories(boost_redis INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
  boost_redis
  INTERFACE
    Boost::asio
    Boost::assert
    Boost::config
    Boost::core
    Boost::mp11
    Boost::system
    Boost::utility
)

target_compile_features(boost_redis INTERFACE cxx_std_17)

# Asio bases C++ feature detection on __cplusplus. Make MSVC
# define it correctly
if (MSVC)
    target_compile_options(boost_redis INTERFACE /Zc:__cplusplus)
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/BoostRedisConfigVersion.cmake"
    COMPATIBILITY AnyNewerVersion
)

find_package(Boost 1.80 REQUIRED)

# We test the protobuf example only on gcc.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
   find_package(Protobuf) # For the protobuf example.
endif()

include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)

enable_testing()
include_directories(include)

# Main function for the examples.
#=======================================================================

add_library(test_common STATIC
   tests/common.cpp
)
target_compile_features(test_common PUBLIC cxx_std_17)
if (MSVC)
    target_compile_options(test_common PRIVATE /bigobj)
    target_compile_definitions(test_common PRIVATE _WIN32_WINNT=0x0601)
endif()

#=======================================================================

add_library(common STATIC
   examples/start.cpp
   examples/main.cpp
   examples/boost_redis.cpp
)
target_compile_features(common PUBLIC cxx_std_20)
if (MSVC)
    target_compile_options(common PRIVATE /bigobj)
    target_compile_definitions(common PRIVATE _WIN32_WINNT=0x0601)
endif()

# Executables
#=======================================================================

add_executable(cpp20_intro examples/cpp20_intro.cpp)
target_link_libraries(cpp20_intro common)
target_compile_features(cpp20_intro PUBLIC cxx_std_20)
add_test(cpp20_intro cpp20_intro)
if (MSVC)
    target_compile_options(cpp20_intro PRIVATE /bigobj)
    target_compile_definitions(cpp20_intro PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(cpp20_streams examples/cpp20_streams.cpp)
target_link_libraries(cpp20_streams common)
target_compile_features(cpp20_streams PUBLIC cxx_std_20)
if (MSVC)
    target_compile_options(cpp20_streams PRIVATE /bigobj)
    target_compile_definitions(cpp20_streams PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(cpp17_intro examples/cpp17_intro.cpp)
target_compile_features(cpp17_intro PUBLIC cxx_std_17)
add_test(cpp17_intro cpp17_intro)
if (MSVC)
    target_compile_options(cpp17_intro PRIVATE /bigobj)
    target_compile_definitions(cpp17_intro PRIVATE _WIN32_WINNT=0x0601)
endif()

if (NOT MSVC)
add_executable(cpp17_intro_sync examples/cpp17_intro_sync.cpp)
target_compile_features(cpp17_intro_sync PUBLIC cxx_std_17)
add_test(cpp17_intro_sync cpp17_intro_sync)
endif()

if (NOT MSVC)
add_executable(cpp20_chat_room examples/cpp20_chat_room.cpp)
target_compile_features(cpp20_chat_room PUBLIC cxx_std_20)
target_link_libraries(cpp20_chat_room common)
endif()

add_executable(cpp20_containers examples/cpp20_containers.cpp)
target_compile_features(cpp20_containers PUBLIC cxx_std_20)
target_link_libraries(cpp20_containers common)
add_test(cpp20_containers cpp20_containers)
if (MSVC)
    target_compile_options(cpp20_containers PRIVATE /bigobj)
    target_compile_definitions(cpp20_containers PRIVATE _WIN32_WINNT=0x0601)
endif()

if (NOT MSVC)
add_executable(cpp20_echo_server examples/cpp20_echo_server.cpp)
target_compile_features(cpp20_echo_server PUBLIC cxx_std_20)
target_link_libraries(cpp20_echo_server common)
endif()

add_executable(cpp20_resolve_with_sentinel examples/cpp20_resolve_with_sentinel.cpp)
target_compile_features(cpp20_resolve_with_sentinel PUBLIC cxx_std_20)
target_link_libraries(cpp20_resolve_with_sentinel common)
#add_test(cpp20_resolve_with_sentinel cpp20_resolve_with_sentinel)
if (MSVC)
    target_compile_options(cpp20_resolve_with_sentinel PRIVATE /bigobj)
    target_compile_definitions(cpp20_resolve_with_sentinel PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(cpp20_json examples/cpp20_json.cpp)
target_compile_features(cpp20_json PUBLIC cxx_std_20)
target_link_libraries(cpp20_json common)
add_test(cpp20_json cpp20_json)
if (MSVC)
    target_compile_options(cpp20_json PRIVATE /bigobj)
    target_compile_definitions(cpp20_json PRIVATE _WIN32_WINNT=0x0601)
endif()

if (Protobuf_FOUND)
   protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS examples/person.proto)
   add_executable(cpp20_protobuf examples/cpp20_protobuf.cpp ${PROTO_SRCS} ${PROTO_HDRS})
   target_compile_features(cpp20_protobuf PUBLIC cxx_std_20)
   target_link_libraries(cpp20_protobuf common ${Protobuf_LIBRARIES})
   target_include_directories(cpp20_protobuf PUBLIC ${Protobuf_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
   add_test(cpp20_protobuf cpp20_protobuf)
   if (MSVC)
       target_compile_options(cpp20_protobuf PRIVATE /bigobj)
       target_compile_definitions(cpp20_protobuf PRIVATE _WIN32_WINNT=0x0601)
   endif()
endif()

add_executable(cpp20_subscriber examples/cpp20_subscriber.cpp)
target_compile_features(cpp20_subscriber PUBLIC cxx_std_20)
target_link_libraries(cpp20_subscriber common)
if (MSVC)
    target_compile_options(cpp20_subscriber PRIVATE /bigobj)
    target_compile_definitions(cpp20_subscriber PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(cpp20_intro_tls examples/cpp20_intro_tls.cpp)
target_compile_features(cpp20_intro_tls PUBLIC cxx_std_20)
add_test(cpp20_intro_tls cpp20_intro_tls)
target_link_libraries(cpp20_intro_tls OpenSSL::Crypto OpenSSL::SSL)
target_link_libraries(cpp20_intro_tls common)
if (MSVC)
    target_compile_options(cpp20_intro_tls PRIVATE /bigobj)
    target_compile_definitions(cpp20_intro_tls PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(cpp20_low_level_async tests/cpp20_low_level_async.cpp)
target_compile_features(cpp20_low_level_async PUBLIC cxx_std_20)
add_test(cpp20_low_level_async cpp20_low_level_async)
target_link_libraries(cpp20_low_level_async common)
if (MSVC)
    target_compile_options(cpp20_low_level_async PRIVATE /bigobj)
    target_compile_definitions(cpp20_low_level_async PRIVATE _WIN32_WINNT=0x0601)
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

add_executable(cpp17_low_level_sync tests/cpp17_low_level_sync.cpp)
target_compile_features(cpp17_low_level_sync PUBLIC cxx_std_17)
add_test(cpp17_low_level_sync cpp17_low_level_sync)
if (MSVC)
    target_compile_options(cpp17_low_level_sync PRIVATE /bigobj)
    target_compile_definitions(cpp17_low_level_sync PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_exec tests/conn_exec.cpp)
target_compile_features(test_conn_exec PUBLIC cxx_std_20)
target_link_libraries(test_conn_exec test_common)
add_test(test_conn_exec test_conn_exec)
if (MSVC)
    target_compile_options(test_conn_exec PRIVATE /bigobj)
    target_compile_definitions(test_conn_exec PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_exec_retry tests/conn_exec_retry.cpp)
target_compile_features(test_conn_exec_retry PUBLIC cxx_std_20)
target_link_libraries(test_conn_exec_retry test_common)
add_test(test_conn_exec_retry test_conn_exec_retry)
if (MSVC)
    target_compile_options(test_conn_exec_retry PRIVATE /bigobj)
    target_compile_definitions(test_conn_exec_retry PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_push tests/conn_push.cpp)
target_compile_features(test_conn_push PUBLIC cxx_std_20)
target_link_libraries(test_conn_push test_common)
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

add_executable(test_conn_reconnect tests/conn_reconnect.cpp)
target_compile_features(test_conn_reconnect PUBLIC cxx_std_20)
target_link_libraries(test_conn_reconnect common test_common)
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
target_link_libraries(test_conn_exec_cancel common test_common)
add_test(test_conn_exec_cancel test_conn_exec_cancel)
if (MSVC)
    target_compile_options(test_conn_exec_cancel PRIVATE /bigobj)
    target_compile_definitions(test_conn_exec_cancel PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_exec_cancel2 tests/conn_exec_cancel2.cpp)
target_compile_features(test_conn_exec_cancel2 PUBLIC cxx_std_20)
target_link_libraries(test_conn_exec_cancel2 common test_common)
add_test(test_conn_exec_cancel2 test_conn_exec_cancel2)
if (MSVC)
    target_compile_options(test_conn_exec_cancel2 PRIVATE /bigobj)
    target_compile_definitions(test_conn_exec_cancel2 PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_exec_error tests/conn_exec_error.cpp)
target_compile_features(test_conn_exec_error PUBLIC cxx_std_17)
target_link_libraries(test_conn_exec_error common test_common)
add_test(test_conn_exec_error test_conn_exec_error)
if (MSVC)
    target_compile_options(test_conn_exec_error PRIVATE /bigobj)
    target_compile_definitions(test_conn_exec_error PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_echo_stress tests/conn_echo_stress.cpp)
target_compile_features(test_conn_echo_stress PUBLIC cxx_std_20)
target_link_libraries(test_conn_echo_stress common test_common)
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

add_executable(test_issue_50 tests/issue_50.cpp)
target_compile_features(test_issue_50 PUBLIC cxx_std_20)
target_link_libraries(test_issue_50 common)
add_test(test_issue_50 test_issue_50)
if (MSVC)
    target_compile_options(test_issue_50 PRIVATE /bigobj)
    target_compile_definitions(test_issue_50 PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_conn_check_health tests/conn_check_health.cpp)
target_compile_features(test_conn_check_health PUBLIC cxx_std_17)
target_link_libraries(test_conn_check_health common)
add_test(test_conn_check_health test_conn_check_health)
if (MSVC)
    target_compile_options(test_conn_check_health PRIVATE /bigobj)
    target_compile_definitions(test_conn_check_health PRIVATE _WIN32_WINNT=0x0601)
endif()

add_executable(test_run tests/run.cpp)
target_compile_features(test_run PUBLIC cxx_std_17)
target_link_libraries(test_run test_common)
add_test(test_run test_run)
if (MSVC)
    target_compile_options(test_run PRIVATE /bigobj)
    target_compile_definitions(test_run PRIVATE _WIN32_WINNT=0x0601)
endif()

# Install
#=======================================================================

install(TARGETS boost_redis
    EXPORT boost_redis
    PUBLIC_HEADER DESTINATION include COMPONENT Development
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
   "${PROJECT_SOURCE_DIR}/cmake/BoostRedisConfig.cmake.in"
   "${PROJECT_BINARY_DIR}/BoostRedisConfig.cmake"
    INSTALL_DESTINATION lib/cmake/boost/redis
)

install(EXPORT boost_redis DESTINATION lib/cmake/boost/redis)
install(FILES "${PROJECT_BINARY_DIR}/BoostRedisConfigVersion.cmake"
   "${PROJECT_BINARY_DIR}/BoostRedisConfig.cmake"
     DESTINATION lib/cmake/boost/redis)

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

