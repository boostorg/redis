include(CMakeFindDependencyMacro)

find_dependency(Boost 1.74 COMPONENTS system REQUIRED)
find_dependency(fmt 7.0 REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/aedis-targets.cmake")
