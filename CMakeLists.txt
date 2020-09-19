cmake_minimum_required(VERSION 3.15)

project(aedis CXX)

option(AEDIS_BUILD_EXAMPLES "Build aedis examples" ON)
option(AEDIS_BUILD_TESTS "Build aedis tests" ON)
option(AEDIS_USE_CONAN "Include the conan build info file" ON)

include(GNUInstallDirs)

if(AEDIS_USE_CONAN)
	include("${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
	conan_basic_setup(TARGETS)
	list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/find_conan_packages")
endif()

find_package(Boost 1.74 REQUIRED COMPONENTS system)
find_package(fmt 7.0 REQUIRED)

add_library(aedis INTERFACE)
add_library(aedis::aedis ALIAS aedis)

target_link_libraries(aedis INTERFACE fmt::fmt Boost::system)
target_compile_features(aedis INTERFACE cxx_std_17)

target_include_directories(aedis INTERFACE
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

if(AEDIS_BUILD_EXAMPLES)
	add_executable(examples examples.cpp)
	target_link_libraries(examples PRIVATE aedis::aedis)
endif()

if(AEDIS_BUILD_TESTS)
	enable_testing()
	add_executable(tests tests.cpp)
	target_link_libraries(tests PRIVATE aedis::aedis)
	add_test(NAME aedis_tests COMMAND tests)
endif()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/cmake/aedis-config.cmake" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
install(TARGETS aedis EXPORT aedis-targets)
install(FILES aedis.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
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
