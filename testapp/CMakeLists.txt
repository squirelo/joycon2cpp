cmake_minimum_required(VERSION 3.20)
project(testapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include_directories(
  src
)

file(GLOB_RECURSE SRC_FILES
  src/*.cpp
)

add_executable(testapp ${SRC_FILES})

# Link Windows Runtime libraries
target_link_libraries(testapp PRIVATE 
  windowsapp
  runtimeobject
)

if(MSVC)
  target_compile_options(testapp PRIVATE /W3 /permissive-)
else()
  target_compile_options(testapp PRIVATE -Wall -Wextra -pedantic)
endif()
