cmake_minimum_required(VERSION 3.20)

project(build)

add_library(build INTERFACE
	build.hpp)

target_compile_features(build INTERFACE cxx_std_20)

target_include_directories(build INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(build INTERFACE win32_helper)
