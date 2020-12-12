cmake_minimum_required(VERSION 3.18)

project(aedis CXX)

option(AEDIS_BUILD_EXAMPLES "Build aedis examples" ON)
option(AEDIS_BUILD_TESTS "Build aedis tests" ON)
option(AEDIS_USE_CONAN "Include the conan build info file" OFF)

include(GNUInstallDirs)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")

#target_compile_features(aedis INTERFACE cxx_std_20)
#add_definitions(-D BOOST_ASIO_CONCURRENCY_HINT_1=BOOST_ASIO_CONCURRENCY_HINT_UNSAFE)
#add_definitions(-D BOOST_ASIO_NO_DEPRECATED)
#add_definitions(-D BOOST_ASIO_NO_TS_EXECUTORS)

if(AEDIS_USE_CONAN)
	include("${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
	conan_basic_setup(TARGETS)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_conan_packages")
endif()

find_package(Boost 1.74 REQUIRED COMPONENTS system)

add_library(aedis INTERFACE)
add_library(aedis::aedis ALIAS aedis)

target_include_directories(aedis INTERFACE
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
)

if(AEDIS_BUILD_EXAMPLES)
	add_executable(example examples/async.cpp)
	target_link_libraries(example PRIVATE aedis::aedis)
endif()

if(AEDIS_BUILD_TESTS)
	enable_testing()
	add_executable(general tests/general.cpp)
	target_link_libraries(general PRIVATE aedis::aedis)
	add_test(NAME aedis_test COMMAND general)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aedis-config.cmake" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
install(TARGETS aedis EXPORT aedis-targets)
install(FILES include/aedis/aedis.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aedis-config.cmake" DESTINATION share/cmake/aedis)
install(EXPORT aedis-targets
	FILE aedis-targets.cmake
	NAMESPACE aedis::
	DESTINATION share/cmake/aedis
)

export(
	EXPORT aedis-targets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/aedis-targets.cmake"
	NAMESPACE aedis::
)

