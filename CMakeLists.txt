cmake_minimum_required(VERSION 3.8...4.2)

project(boost_redis VERSION "${BOOST_SUPERPROJECT_VERSION}" LANGUAGES CXX)

# Dependencies
# Boost dependencies should be already available.
# If other dependencies are not found, we bail out
find_package(Threads)
if(NOT Threads_FOUND)
  message(STATUS "Boost.Redis has been disabled, because the required package Threads hasn't been found")
  return()
endif()
find_package(OpenSSL)
if(NOT OpenSSL_FOUND)
  message(STATUS "Boost.Redis has been disabled, because the required package OpenSSL hasn't been found")
  return()
endif()

# Library (Asio)
add_library(boost_redis INTERFACE)
add_library(Boost::redis ALIAS boost_redis)
target_include_directories(boost_redis INTERFACE include)
target_compile_features(boost_redis INTERFACE cxx_std_17)
target_link_libraries(boost_redis
  INTERFACE
    Boost::asio
    Boost::assert
    Boost::core
    Boost::mp11
    Boost::system
    Boost::throw_exception
    Threads::Threads
    OpenSSL::Crypto
    OpenSSL::SSL
)

# Library (Corosio)
add_library(boost_redis_corosio INTERFACE)
add_library(Boost::redis_corosio ALIAS boost_redis_corosio)
target_include_directories(boost_redis_corosio INTERFACE include)
target_compile_features(boost_redis_corosio INTERFACE cxx_std_20)
target_link_libraries(boost_redis_corosio
  INTERFACE
    Boost::assert
    Boost::core
    Boost::mp11
    Boost::system
    Boost::throw_exception
    Boost::capy
    Boost::corosio
    Boost::corosio_openssl
)

# Don't run integration testing unless explicitly requested, since these require a running server
option(BOOST_REDIS_INTEGRATION_TESTS OFF "Whether to build and run integration tests or not")
mark_as_advanced(BOOST_REDIS_INTEGRATION_TESTS)

# Examples and tests
if(BUILD_TESTING)
  # Custom target tests; required by the Boost superproject
  if(NOT TARGET tests)
    add_custom_target(tests)
  endif()

  # Tests and common utilities
  add_subdirectory(test)

  # Benchmarks. Build them with tests to prevent code rotting
  add_subdirectory(benchmarks)

  # Examples. All of them require a real server running
  if (BOOST_REDIS_INTEGRATION_TESTS)
    add_subdirectory(example)
  endif()
endif()

if(BOOST_SUPERPROJECT_VERSION AND NOT CMAKE_VERSION VERSION_LESS 3.13)
  boost_install(
    TARGETS boost_redis_corosio
    VERSION ${BOOST_SUPERPROJECT_VERSION}
    HEADER_DIRECTORY include
  )
endif()
