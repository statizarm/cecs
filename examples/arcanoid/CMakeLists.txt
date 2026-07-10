cmake_minimum_required(VERSION 4.0)
project(cecs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

add_executable(
  arcanoid
  src/main.cpp
)

target_link_libraries(
  arcanoid
  SFML::Graphics
  SFML::Audio
)

file(
  COPY
    assets
  DESTINATION
    ${CMAKE_CURRENT_BINARY_DIR}
)