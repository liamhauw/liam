cmake_minimum_required(VERSION 3.20...3.25)

project(
  Liam VERSION 0.1.0
  LANGUAGES CXX
)

set(LIAM_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${LIAM_DIR}/bin")
endif()

add_subdirectory(extern)
add_subdirectory(shaders)
add_subdirectory(src)
