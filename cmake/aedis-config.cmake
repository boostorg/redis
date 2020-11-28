include(CMakeFindDependencyMacro)

find_dependency(Boost 1.74 COMPONENTS system REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/aedis-targets.cmake")
