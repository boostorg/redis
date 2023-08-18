cmake_minimum_required(VERSION 3.14)

#set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CMAKE_COMMAND} -E time")

# determine whether it's main/root project
# or being built under another project.
if (NOT DEFINED BOOST_REDIS_MAIN_PROJECT)
   set(BOOST_REDIS_MAIN_PROJECT OFF)
   if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
      set(BOOST_REDIS_MAIN_PROJECT ON)
   endif()
endif()

project(
    boost_redis
    VERSION 1.4.1
    DESCRIPTION "A redis client library"
    HOMEPAGE_URL "https://boostorg.github.io/redis/"
    LANGUAGES CXX
)

option(BOOST_REDIS_INSTALL "Generate install targets." ${BOOST_REDIS_MAIN_PROJECT})
option(BOOST_REDIS_TESTS "Build tests." ${BOOST_REDIS_MAIN_PROJECT})
option(BOOST_REDIS_EXAMPLES "Build examples." ${BOOST_REDIS_MAIN_PROJECT})
option(BOOST_REDIS_BENCHMARKS "Build benchmarks." ${BOOST_REDIS_MAIN_PROJECT})
option(BOOST_REDIS_DOC "Generate documentations." ${BOOST_REDIS_MAIN_PROJECT})

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

find_package(Boost 1.80 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)

include_directories(include)

# Common
#=======================================================================

add_library(boost_redis_project_options INTERFACE)
target_link_libraries(boost_redis_project_options INTERFACE OpenSSL::Crypto OpenSSL::SSL)
if (MSVC)
   target_compile_options(boost_redis_project_options INTERFACE /bigobj)
   target_compile_definitions(boost_redis_project_options INTERFACE _WIN32_WINNT=0x0601)
endif()

add_library(boost_redis_src STATIC examples/boost_redis.cpp)
target_compile_features(boost_redis_src PRIVATE cxx_std_17)
target_link_libraries(boost_redis_src PRIVATE boost_redis_project_options)

# Executables
#=======================================================================

if (BOOST_REDIS_BENCHMARKS)
  add_library(benchmarks_options INTERFACE)
  target_link_libraries(benchmarks_options INTERFACE boost_redis_src)
  target_link_libraries(benchmarks_options INTERFACE boost_redis_project_options)
  target_compile_features(benchmarks_options INTERFACE cxx_std_20)

  add_executable(echo_server_client benchmarks/cpp/asio/echo_server_client.cpp)
  target_link_libraries(echo_server_client PRIVATE benchmarks_options)

  add_executable(echo_server_direct benchmarks/cpp/asio/echo_server_direct.cpp)
  target_link_libraries(echo_server_direct PRIVATE benchmarks_options)
endif()

if (BOOST_REDIS_EXAMPLES)
  add_library(examples_main STATIC examples/main.cpp)
  target_compile_features(examples_main PRIVATE cxx_std_20)
  target_link_libraries(examples_main PRIVATE boost_redis_project_options)

  macro(make_example EXAMPLE_NAME STANDARD)
    add_executable(${EXAMPLE_NAME} examples/${EXAMPLE_NAME}.cpp)
    target_link_libraries(${EXAMPLE_NAME} PRIVATE boost_redis_src)
    target_link_libraries(${EXAMPLE_NAME} PRIVATE boost_redis_project_options)
    target_compile_features(${EXAMPLE_NAME} PRIVATE cxx_std_${STANDARD})
    if (${STANDARD} STREQUAL "20")
      target_link_libraries(${EXAMPLE_NAME} PRIVATE examples_main)
    endif()
  endmacro()

  macro(make_testable_example EXAMPLE_NAME STANDARD)
    make_example(${EXAMPLE_NAME} ${STANDARD})
    add_test(${EXAMPLE_NAME} ${EXAMPLE_NAME})
  endmacro()

  make_testable_example(cpp17_intro 17)
  make_testable_example(cpp17_intro_sync 17)

  make_testable_example(cpp20_intro 20)
  make_testable_example(cpp20_containers 20)
  make_testable_example(cpp20_json 20)
  make_testable_example(cpp20_intro_tls 20)

  make_example(cpp20_subscriber 20)
  make_example(cpp20_streams 20)
  make_example(cpp20_echo_server 20)
  make_example(cpp20_resolve_with_sentinel 20)

  # We test the protobuf example only on gcc.
  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    find_package(Protobuf)
    if (Protobuf_FOUND)
       protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS examples/person.proto)
       make_testable_example(cpp20_protobuf 20)
       target_sources(cpp20_protobuf PUBLIC ${PROTO_SRCS} ${PROTO_HDRS})
       target_link_libraries(cpp20_protobuf PRIVATE  ${Protobuf_LIBRARIES})
       target_include_directories(cpp20_protobuf PUBLIC ${Protobuf_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  endif()

  if (NOT MSVC)
     make_example(cpp20_chat_room 20)
  endif()
endif()

if (BOOST_REDIS_TESTS)
  enable_testing()

  add_library(tests_common STATIC tests/common.cpp)
  target_compile_features(tests_common PRIVATE cxx_std_17)
  target_link_libraries(tests_common PRIVATE boost_redis_project_options)

  macro(make_test TEST_NAME STANDARD)
    add_executable(${TEST_NAME} tests/${TEST_NAME}.cpp)
    target_link_libraries(${TEST_NAME} PRIVATE boost_redis_src tests_common)
    target_link_libraries(${TEST_NAME} PRIVATE boost_redis_project_options)
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_${STANDARD})
    add_test(${TEST_NAME} ${TEST_NAME})
  endmacro()

  make_test(test_conn_quit 17)
  make_test(test_conn_tls 17)
  make_test(test_low_level 17)
  make_test(test_conn_exec_retry 17)
  make_test(test_conn_exec_error 17)
  make_test(test_request 17)
  make_test(test_run 17)
  make_test(test_low_level_sync 17)
  make_test(test_low_level_sync_sans_io 17)
  make_test(test_conn_check_health 17)

  make_test(test_conn_exec 20)
  make_test(test_conn_push 20)
  make_test(test_conn_reconnect 20)
  make_test(test_conn_exec_cancel 20)
  make_test(test_conn_exec_cancel2 20)
  make_test(test_conn_echo_stress 20)
  make_test(test_low_level_async 20)
  make_test(test_conn_run_cancel 20)
  make_test(test_issue_50 20)
endif()

# Install
#=======================================================================

if (BOOST_REDIS_INSTALL)
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

  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/BoostRedisConfigVersion.cmake"
    COMPATIBILITY AnyNewerVersion
  )

  include(CPack)
endif()

# Doxygen
#=======================================================================

if (BOOST_REDIS_DOC)
  set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/doc")
  configure_file(doc/Doxyfile.in doc/Doxyfile @ONLY)
  
  add_custom_target(
      doc
      COMMAND doxygen "${PROJECT_BINARY_DIR}/doc/Doxyfile"
      COMMENT "Building documentation using Doxygen"
      WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
      VERBATIM
  )
endif()

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

# TODO
#=======================================================================

#.PHONY: bench
#bench:
#	pdflatex --jobname=echo-f0 benchmarks/benchmarks.tex
#	pdflatex --jobname=echo-f1 benchmarks/benchmarks.tex
# pdftoppm {input.pdf} {output.file} -png

