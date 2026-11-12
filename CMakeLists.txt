cmake_minimum_required(VERSION 3.16)
project(BrainSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)  # the 1000-neuron network wants optimization
endif()

add_executable(brain src/main.cpp)
target_include_directories(brain PRIVATE include)

if(MSVC)
  target_compile_options(brain PRIVATE /W4)
else()
  target_compile_options(brain PRIVATE -Wall -Wextra -Wpedantic)
endif()
