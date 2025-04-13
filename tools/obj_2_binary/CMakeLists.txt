cmake_minimum_required(VERSION 3.13) # CMake version check
project(obj_2_binary)                         
set(CMAKE_CXX_STANDARD 20)           # Enable C++20 standard

add_executable(obj_2_binary "obj_2_binary.cpp")

target_include_directories(obj_2_binary PRIVATE ${CMAKE_SOURCE_DIR})
target_include_directories(obj_2_binary PRIVATE ${CMAKE_SOURCE_DIR}/../../external)
target_include_directories(obj_2_binary PRIVATE ${CMAKE_SOURCE_DIR}/../..)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} \
    -g -O0"
  )

target_sources(obj_2_binary PRIVATE 
  ${CMAKE_SOURCE_DIR}/../../math/vec.cpp
  ${CMAKE_SOURCE_DIR}/../../math/mat4.cpp
  ${CMAKE_SOURCE_DIR}/../../math/quaternion.cpp
  ${CMAKE_SOURCE_DIR}/../../math/vec.h
  ${CMAKE_SOURCE_DIR}/../../math/mat4.h
  ${CMAKE_SOURCE_DIR}/../../math/quaternion.h
)

target_sources(obj_2_binary PRIVATE 
  ${CMAKE_SOURCE_DIR}/../../utils/LogPrint.cpp
  ${CMAKE_SOURCE_DIR}/../../utils/LogPrint.h
)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)


