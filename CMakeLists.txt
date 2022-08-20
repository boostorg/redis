# At the moment the official build system is still autotools and this
# file is meant to support Aedis on windows.

cmake_minimum_required(VERSION 3.14)

project(
    Aedis
    VERSION 0.3.0
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
add_executable(low_level_sync tests/low_level_sync.cpp)

add_test(containers containers)
add_test(intro intro)
add_test(intro_sync intro_sync)
add_test(serialization serialization)
add_test(test_low_level test_low_level)
add_test(test_connection test_connection)
add_test(low_level_sync low_level_sync)

include(GNUInstallDirs)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/boost
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
   FILES_MATCHING
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

