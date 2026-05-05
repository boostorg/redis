# Report whether we're building the Corosio tests or not, for CI traceability
if (NOT BOOST_REDIS_COROSIO_TESTS)
  message(STATUS "Skipping Corosio tests and examples because BOOST_REDIS_COROSIO_TESTS is not defined")
endif()


# Common utilities
add_library(boost_redis_project_options INTERFACE)
if (MSVC)
  # C4459: name hides outer scope variable is issued by Asio
  target_compile_options(boost_redis_project_options INTERFACE /bigobj /W4 /WX /wd4459)
  target_compile_definitions(boost_redis_project_options INTERFACE _WIN32_WINNT=0x0601 _CRT_SECURE_NO_WARNINGS=1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(boost_redis_project_options INTERFACE -Wall -Wextra)
endif()

# Source libraries
add_library(boost_redis_proto_src STATIC boost_redis_proto.cpp)
target_link_libraries(boost_redis_proto_src PUBLIC
  boost_redis_proto
  boost_redis_project_options
)

add_library(boost_redis_asio_src STATIC boost_redis_asio.cpp)
target_link_libraries(boost_redis_asio_src PUBLIC
  boost_redis
  boost_redis_proto_src
)

add_library(boost_redis_corosio_src STATIC boost_redis_corosio.cpp)
target_link_libraries(boost_redis_corosio_src PUBLIC
  boost_redis_corosio
  boost_redis_proto_src
)

# Test utils
add_library(boost_redis_tests_proto STATIC common.cpp sansio_utils.cpp)
target_link_libraries(boost_redis_tests_proto PUBLIC boost_redis_proto_src)
target_compile_definitions(boost_redis_tests_proto PUBLIC BOOST_ALLOW_DEPRECATED=1) # we need to still test deprecated fns

add_library(boost_redis_tests_asio STATIC asio_common.cpp)
target_link_libraries(boost_redis_tests_asio
  boost_redis_asio_src
  boost_redis_tests_proto
)

add_library(boost_redis_tests_corosio STATIC corosio_common.cpp)
target_link_libraries(boost_redis_tests_corosio PUBLIC
  boost_redis_corosio_src
  boost_redis_tests_proto
)

function(boost_redis_make_test TEST_NAME)
  set(EXE_NAME "boost_redis_${TEST_NAME}")
  add_executable(${EXE_NAME} ${TEST_NAME}.cpp)
  if (ARGN)
    target_link_libraries(${EXE_NAME} PRIVATE ${ARGN})
  endif()
  add_test(NAME ${EXE_NAME} COMMAND ${EXE_NAME})
  add_dependencies(tests ${EXE_NAME})
endfunction()

# Unit tests
boost_redis_make_test(test_low_level                boost_redis_tests_proto)
boost_redis_make_test(test_request                  boost_redis_tests_proto)
boost_redis_make_test(test_serialization            boost_redis_tests_proto)
boost_redis_make_test(test_low_level_sync_sans_io   boost_redis_tests_proto)
boost_redis_make_test(test_any_adapter              boost_redis_tests_proto)
boost_redis_make_test(test_log_to_file              boost_redis_tests_proto)
boost_redis_make_test(test_exec_fsm                 boost_redis_tests_proto)
boost_redis_make_test(test_exec_one_fsm             boost_redis_tests_proto)
boost_redis_make_test(test_writer_fsm               boost_redis_tests_proto Boost::asio)
boost_redis_make_test(test_reader_fsm               boost_redis_tests_proto Boost::asio)
boost_redis_make_test(test_sentinel_resolve_fsm     boost_redis_tests_proto Boost::asio)
boost_redis_make_test(test_run_fsm                  boost_redis_tests_proto Boost::asio)
boost_redis_make_test(test_compose_setup_request    boost_redis_tests_proto)
boost_redis_make_test(test_setup_adapter            boost_redis_tests_proto)
boost_redis_make_test(test_multiplexer              boost_redis_tests_proto)
boost_redis_make_test(test_parse_sentinel_response  boost_redis_tests_proto)
boost_redis_make_test(test_update_sentinel_list     boost_redis_tests_proto)
boost_redis_make_test(test_flat_tree                boost_redis_tests_proto)
boost_redis_make_test(test_generic_flat_response    boost_redis_tests_proto)
boost_redis_make_test(test_read_buffer              boost_redis_tests_proto)
boost_redis_make_test(test_subscription_tracker     boost_redis_tests_proto)
boost_redis_make_test(test_push_parser              boost_redis_tests_proto)

boost_redis_make_test(test_conn_logging             boost_redis_tests_asio)
boost_redis_make_test(test_connect_fsm              boost_redis_tests_asio)
boost_redis_make_test(test_receive_fsm              boost_redis_tests_asio)

if (BOOST_REDIS_COROSIO_TESTS)
  boost_redis_make_test(test_co_logging               boost_redis_tests_corosio)
  boost_redis_make_test(test_flow_controller          boost_redis_tests_corosio)
  boost_redis_make_test(test_co_connect               boost_redis_tests_corosio)
  boost_redis_make_test(test_mix_asio_corosio         boost_redis boost_redis_corosio)
endif()

# Tests that require a real Redis server
if (BOOST_REDIS_INTEGRATION_TESTS)
  boost_redis_make_test(test_conn_quit          boost_redis_tests_asio)
  boost_redis_make_test(test_conn_exec_retry    boost_redis_tests_asio)
  boost_redis_make_test(test_conn_exec_error    boost_redis_tests_asio)
  boost_redis_make_test(test_run                boost_redis_tests_asio)
  boost_redis_make_test(test_conn_run_cancel    boost_redis_tests_asio)
  boost_redis_make_test(test_conn_check_health  boost_redis_tests_asio)
  boost_redis_make_test(test_conn_exec          boost_redis_tests_asio)
  boost_redis_make_test(test_conn_push          boost_redis_tests_asio)
  boost_redis_make_test(test_conn_push2         boost_redis_tests_asio)
  boost_redis_make_test(test_conn_monitor       boost_redis_tests_asio)
  boost_redis_make_test(test_conn_reconnect     boost_redis_tests_asio)
  boost_redis_make_test(test_conn_exec_cancel   boost_redis_tests_asio)
  boost_redis_make_test(test_conn_echo_stress   boost_redis_tests_asio)
  boost_redis_make_test(test_conn_move          boost_redis_tests_asio)
  boost_redis_make_test(test_conn_setup         boost_redis_tests_asio)
  boost_redis_make_test(test_issue_50           boost_redis_tests_asio)
  boost_redis_make_test(test_conversions        boost_redis_tests_asio)
  boost_redis_make_test(test_conn_tls           boost_redis_tests_asio)
  boost_redis_make_test(test_unix_sockets       boost_redis_tests_asio)
  boost_redis_make_test(test_conn_cancel_after  boost_redis_tests_asio)
  boost_redis_make_test(test_conn_sentinel      boost_redis_tests_asio)

  if (BOOST_REDIS_COROSIO_TESTS)
    boost_redis_make_test(test_co_run_cancel      boost_redis_tests_corosio)
    boost_redis_make_test(test_co_check_health    boost_redis_tests_corosio)
    boost_redis_make_test(test_co_push2           boost_redis_tests_corosio)
    boost_redis_make_test(test_co_exec_cancel     boost_redis_tests_corosio)
    boost_redis_make_test(test_co_move            boost_redis_tests_corosio)
    boost_redis_make_test(test_co_setup           boost_redis_tests_corosio)
    boost_redis_make_test(test_co_tls             boost_redis_tests_corosio)
    boost_redis_make_test(test_co_unix_sockets    boost_redis_tests_corosio)
    boost_redis_make_test(test_co_sentinel        boost_redis_tests_corosio)
  endif()

endif()