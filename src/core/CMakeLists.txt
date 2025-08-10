# Core library: auto-collect all headers/sources under this folder

# Collect sources/headers recursively. CONFIGURE_DEPENDS makes CMake re-scan when files change.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)
file(GLOB_RECURSE CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

add_library(luster_core ${CORE_SOURCES} ${CORE_HEADERS})
add_library(luster::core ALIAS luster_core)

# Public include so that `#include "core/..."` works from targets that link to this
target_include_directories(luster_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

# Core depends on third-party headers (SDL, Vulkan, spdlog). Reuse top-level interface target.
if(TARGET luster::thirdparty)
  target_link_libraries(luster_core PUBLIC luster::thirdparty)
endif()

if(MSVC)
  target_compile_options(luster_core PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
  target_compile_options(luster_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Runtime toggles via compile definitions
# Profiling: default ON in Debug, OFF otherwise
target_compile_definitions(luster_core PUBLIC
  $<$<CONFIG:Debug>:LUSTER_ENABLE_PROFILING=1>
  $<$<NOT:$<CONFIG:Debug>>:LUSTER_ENABLE_PROFILING=0>
)

# Log level: 2=info in Debug, 3=warn otherwise (0=trace,1=debug,2=info,3=warn,4=err,5=critical,6=off)
target_compile_definitions(luster_core PUBLIC
  $<$<CONFIG:Debug>:LUSTER_LOG_LEVEL=2>
  $<$<NOT:$<CONFIG:Debug>>:LUSTER_LOG_LEVEL=3>
)

# Better grouping in IDEs like Visual Studio
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${CORE_SOURCES} ${CORE_HEADERS})


