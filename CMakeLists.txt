cmake_minimum_required(VERSION 3.16)
project(BrainSimulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)  # the 1000-neuron network wants optimization
endif()

# --- Terminal version (no dependencies) --------------------------------------
add_executable(brain src/main.cpp)
target_include_directories(brain PRIVATE include)

if(MSVC)
  target_compile_options(brain PRIVATE /W4)
else()
  target_compile_options(brain PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Live graphics version (SFML) --------------------------------------------
# Turn on with:  cmake -S . -B build -DBRAIN_LIVE=ON
# SFML is downloaded and built automatically, so it always matches your compiler.
option(BRAIN_LIVE "Build the live SFML visualization (brain-live)" OFF)

if(BRAIN_LIVE)
  cmake_minimum_required(VERSION 3.24)  # needed by SFML 3
  include(FetchContent)
  FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
  set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
  set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(SFML)

  add_executable(brain-live src/live.cpp)
  target_include_directories(brain-live PRIVATE include)
  target_link_libraries(brain-live PRIVATE SFML::Graphics)
  if(MSVC)
    target_compile_options(brain-live PRIVATE /W4)
  else()
    target_compile_options(brain-live PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  if(WIN32)
    # Static runtime on MinGW so brain-live.exe runs without extra DLLs.
    if(MINGW)
      target_link_options(brain-live PRIVATE -static)
    endif()
  endif()
endif()
