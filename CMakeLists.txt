cmake_minimum_required(VERSION 3.21)

# Project
project(luster VERSION 0.1.0 LANGUAGES C CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(ENABLE_WARNINGS "Enable compiler warnings" ON)
# USE_VCPKG is documented via presets; toolchain must be supplied at configure time
option(USE_VCPKG "Use vcpkg-managed dependencies (configure via preset/toolchain; default OFF)" OFF)

# Output dirs
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Warnings
if(ENABLE_WARNINGS)
  if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus)
  else()
    add_compile_options(-Wall -Wextra -Wpedantic)
  endif()
endif()

# Enable compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add cmake modules path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Third-party libraries
if(EXISTS "${CMAKE_SOURCE_DIR}/external/CMakeLists.txt")
  add_subdirectory(external)
else()
  message(STATUS "external/CMakeLists.txt not found; third-party libs will be unavailable until added.")
endif()

# Engine/source
if(EXISTS "${CMAKE_SOURCE_DIR}/src/CMakeLists.txt")
  add_subdirectory(src)
else()
  message(STATUS "src/CMakeLists.txt not found; add sources to build the engine.")
endif()

# Installation (optional, placeholder)
include(GNUInstallDirs OPTIONAL)