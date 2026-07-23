cmake_minimum_required(VERSION 4.0)

project(cecs)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

set(
    INCLUDES
    include/cecs/allocator.hpp
    include/cecs/buffer_layout.hpp
    include/cecs/entity.hpp
    include/cecs/soa.hpp
    include/cecs/type_list.hpp
    include/cecs/utils.hpp
    include/cecs/world.hpp
)

set(
    SRCS
    $INCLUDES
)

add_library(cecs INTERFACE)

target_include_directories(
    cecs INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_options(
    cecs INTERFACE -Wno-invalid-offsetof
)