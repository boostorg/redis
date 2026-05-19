cmake_minimum_required(VERSION 3.5...3.22)

project(boost_redis_corosio_b2_test LANGUAGES CXX)

find_package(Boost REQUIRED COMPONENTS headers capy corosio corosio_openssl)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE
  Boost::headers
  Boost::capy
  Boost::corosio
  Boost::corosio_openssl
)

include(CTest)
add_test(NAME main COMMAND main)
