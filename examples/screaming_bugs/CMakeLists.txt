cmake_minimum_required(VERSION 4.0)

project(cecs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_MakeAvailable(SFML)

add_compile_definitions(EASTL_USER_DEFINED_ALLOCATOR_NEW=0)
add_compile_definitions(EASTL_ALLOCATOR_EXPLICIT_USER_DEFINED_ALLOCATOR_NEW=0)
add_compile_definitions(EASTL_STD_ALLOCATOR_ONLY=1)

FetchContent_Declare(
    EASTL
    GIT_REPOSITORY https://github.com/electronicarts/EASTL.git
    GIT_TAG 3.27.01
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_MakeAvailable(EASTL)

add_executable(
    screaming_bugs
    src/main.cpp
    src/misc.cpp
)

target_link_libraries(
    screaming_bugs
    SFML::Graphics
    EASTL
)
