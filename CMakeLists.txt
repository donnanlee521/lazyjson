cmake_minimum_required(VERSION 3.20)

set(app_name lazyjson)
set(app_version 0.1.0)

project(${app_name} VERSION ${app_version}
    LANGUAGES CXX
    DESCRIPTION "STL friendly c++ json library which implements lazy evaluation and no additional memory allocation")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

file(GLOB srcs src/*.cpp)

add_library(${app_name} SHARED ${srcs})

target_compile_options(${app_name} PUBLIC -O2;-Wnrvo)
target_include_directories(${app_name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(${app_name} PROPERTIES
    PUBLIC_HEADER "include/lazy/lazyjson.hpp;include/lazy/lazyjson_value.hpp;include/lazy/lazyjson_utils.hpp;include/lazy/lazyjson_impl.tpp;include/lazy/lazyjson_concept.hpp"
)

install(TARGETS ${app_name}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/lazy
)