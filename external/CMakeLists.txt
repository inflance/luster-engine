cmake_minimum_required(VERSION 3.21)

# Enforce static builds for bundled deps by default
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build third-party libraries as shared" FORCE)

# -----------------------------
# SDL3 (windowing/input + Vulkan surface)
# -----------------------------
set(SDL_SHARED OFF CACHE BOOL "")
set(SDL_STATIC ON CACHE BOOL "")
set(SDL_TESTS OFF CACHE BOOL "")
set(SDL_INSTALL OFF CACHE BOOL "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/SDL/CMakeLists.txt")
  add_subdirectory(SDL)
  # Targets vary by version/generator; prefer namespaced if available.
  # SDL3::SDL3 is the common imported/alias target; fallback to SDL3.
else()
  message(STATUS "SDL submodule not present at external/SDL")
endif()

# -----------------------------
# volk (Vulkan function loader)
# -----------------------------
set(VOLK_INSTALL OFF CACHE BOOL "")
set(VOLK_BUILD_TESTS OFF CACHE BOOL "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/volk/CMakeLists.txt")
  add_subdirectory(volk)
  if(TARGET volk AND NOT TARGET volk::volk)
    add_library(volk::volk ALIAS volk)
  endif()
else()
  message(STATUS "volk submodule not present at external/volk")
endif()

# -----------------------------
# glm (header-only)
# -----------------------------
set(GLM_TEST_ENABLE OFF CACHE BOOL "")
set(GLM_QUIET ON CACHE BOOL "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/glm/CMakeLists.txt")
  add_subdirectory(glm)
else()
  message(STATUS "glm submodule not present at external/glm")
endif()

# -----------------------------
# spdlog
# -----------------------------
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "")
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "")
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "")
set(SPDLOG_INSTALL OFF CACHE BOOL "")
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/spdlog/CMakeLists.txt")
  add_subdirectory(spdlog)
else()
  message(STATUS "spdlog submodule not present at external/spdlog")
endif()

# -----------------------------
# Vulkan-Headers (headers-only; provides vulkan/vulkan.h)
# -----------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Vulkan-Headers/CMakeLists.txt")
  add_subdirectory(Vulkan-Headers)
else()
  message(STATUS "Vulkan-Headers submodule not present at external/Vulkan-Headers")
endif()

# -----------------------------
# Aggregate interface for easy linking
# -----------------------------
add_library(culkan_thirdparty INTERFACE)

# Link conditionally only if targets exist
if(TARGET volk::volk_headers)
  target_link_libraries(culkan_thirdparty INTERFACE volk::volk_headers)
elseif(TARGET volk_headers)
  target_link_libraries(culkan_thirdparty INTERFACE volk_headers)
endif()

# Vulkan headers target (provide include dirs)
if(TARGET Vulkan::Headers)
  target_link_libraries(culkan_thirdparty INTERFACE Vulkan::Headers)
elseif(TARGET VulkanHeaders)
  target_link_libraries(culkan_thirdparty INTERFACE VulkanHeaders)
endif()

# Vulkan loader (from SDK)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_link_libraries(culkan_thirdparty INTERFACE Vulkan::Vulkan)
else()
  message(STATUS "Vulkan loader not found via find_package(Vulkan); ensure SDK installed or link manually")
endif()

# SDL3 preferred targets
if(TARGET SDL3::SDL3)
  target_link_libraries(culkan_thirdparty INTERFACE SDL3::SDL3)
elseif(TARGET SDL3)
  target_link_libraries(culkan_thirdparty INTERFACE SDL3)
endif()

if(TARGET glm::glm)
  target_link_libraries(culkan_thirdparty INTERFACE glm::glm)
endif()

if(TARGET spdlog::spdlog)
  target_link_libraries(culkan_thirdparty INTERFACE spdlog::spdlog)
endif()

# Provide a namespaced alias
add_library(culkan::thirdparty ALIAS culkan_thirdparty)
add_library(luster::thirdparty ALIAS culkan_thirdparty)