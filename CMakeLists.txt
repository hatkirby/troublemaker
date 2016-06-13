cmake_minimum_required (VERSION 3.1)
project (troublemaker)

set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(vendor/libtwittercpp)
include_directories(vendor/libtwittercpp/src)

add_subdirectory(vendor/yaml-cpp EXCLUDE_FROM_ALL)
include_directories(vendor/yaml-cpp/include)

add_executable(troublemaker troublemaker.cpp)
set_property(TARGET troublemaker PROPERTY CXX_STANDARD 11)
set_property(TARGET troublemaker PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(troublemaker twitter++ yaml-cpp)
