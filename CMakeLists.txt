cmake_minimum_required(VERSION 3.14)

set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

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

#=======================================================================

set(libs_common boost_redis_src)
set(libs_cpp17 tests_common)
set(libs_cpp20 examples_main)

foreach(lib IN LISTS libs_cpp20 libs_cpp17 libs_common)
   add_library(${lib} STATIC)
endforeach()

target_sources(boost_redis_src PRIVATE examples/boost_redis.cpp)
target_sources(tests_common PRIVATE tests/common.cpp)
target_sources(examples_main PRIVATE examples/main.cpp)

# Executables
#=======================================================================

set(benchmakrs echo_server_client echo_server_direct)
add_executable(echo_server_client benchmarks/cpp/asio/echo_server_client.cpp)
add_executable(echo_server_direct benchmarks/cpp/asio/echo_server_direct.cpp)

set(tests_cpp17
   test_conn_quit
   test_conn_tls
   test_low_level
   test_conn_exec_retry
   test_conn_exec_error
   test_request
   test_run
   test_low_level_sync
   test_conn_check_health
)

set(tests_cpp20
    test_conn_exec
    test_conn_push
    test_conn_reconnect
    test_conn_exec_cancel
    test_conn_exec_cancel2
    test_conn_echo_stress
    test_low_level_async
    test_conn_run_cancel
    test_issue_50
)

set(examples_cpp17
    cpp17_intro
    cpp17_intro_sync)

set(examples_cpp20
    cpp20_intro
    cpp20_streams
    cpp20_containers
    cpp20_echo_server
    cpp20_json
    cpp20_subscriber
    cpp20_intro_tls
    cpp20_resolve_with_sentinel
)

if (Protobuf_FOUND)
   list(APPEND examples_cpp20 cpp20_protobuf)
endif()

if (NOT MSVC)
   list(APPEND examples_cpp20 cpp20_chat_room)
endif()

foreach(exe IN LISTS examples_cpp17 examples_cpp20)
   add_executable(${exe} examples/${exe}.cpp)
endforeach()

foreach(exe IN LISTS examples_cpp20)
   target_link_libraries(${exe} PRIVATE examples_main)
endforeach()

foreach(exe IN LISTS tests_cpp17 tests_cpp20)
   add_executable(${exe} tests/${exe}.cpp)
   target_link_libraries(${exe} PRIVATE  tests_common)
endforeach()

foreach(exe IN LISTS tests_cpp17 tests_cpp20 examples_cpp17 examples_cpp20 libs_cpp17 libs_cpp20 libs_common benchmakrs)
   target_link_libraries(${exe} PRIVATE OpenSSL::Crypto OpenSSL::SSL)
   if (MSVC)
      target_compile_options(${exe} PRIVATE /bigobj)
      target_compile_definitions(${exe} PRIVATE _WIN32_WINNT=0x0601)
   endif()
endforeach()

foreach(exe IN LISTS tests_cpp17 tests_cpp20 examples_cpp17 examples_cpp20 libs_cpp17 libs_cpp20 benchmakrs)
   target_link_libraries(${exe} PRIVATE boost_redis_src)
endforeach()

foreach(exe IN LISTS tests_cpp20 examples_cpp20 libs_cpp20 benchmarks)
   target_compile_features(${exe} PUBLIC cxx_std_20)
endforeach()

foreach(exe IN LISTS tests_cpp17 examples_cpp17 libs_cpp17 libs_common)
   target_compile_features(${exe} PUBLIC cxx_std_17)
endforeach()

if (Protobuf_FOUND)
   protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS examples/person.proto)
   target_sources(cpp20_protobuf PUBLIC ${PROTO_SRCS} ${PROTO_HDRS})
   target_link_libraries(cpp20_protobuf PRIVATE  ${Protobuf_LIBRARIES})
   target_include_directories(cpp20_protobuf PUBLIC ${Protobuf_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
   add_test(cpp20_protobuf cpp20_protobuf)
endif()

add_test(cpp17_intro cpp17_intro)
add_test(cpp17_intro_sync cpp17_intro_sync)
add_test(cpp20_intro cpp20_intro)
add_test(cpp20_containers cpp20_containers)
add_test(cpp20_json cpp20_json)
add_test(cpp20_intro_tls cpp20_intro_tls)

foreach(exe IN LISTS tests_cpp17 tests_cpp20)
   add_test(${exe} ${exe})
endforeach()

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

