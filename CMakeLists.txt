cmake_minimum_required(VERSION 3.20)

project(build)

add_library(build INTERFACE
	build.hpp)

target_compile_features(build INTERFACE cxx_std_23)

add_subdirectory(win32_helper)

target_link_libraries(build INTERFACE win32_helper)
