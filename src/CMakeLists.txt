# Engine / App targets
add_executable(luster_sandbox
    main.cpp
    
)

# Group sources in IDEs
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    main.cpp
    
)

# Compile options
if(MSVC)
  target_compile_options(luster_sandbox PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
  target_compile_options(luster_sandbox PRIVATE -Wall -Wextra -Wpedantic)
endif()


# Include + link third-party
target_link_libraries(luster_sandbox PRIVATE luster::thirdparty)

# Set working dir when debugging from IDEs
set_target_properties(luster_sandbox PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# Compile GLSL shaders to SPIR-V (requires Vulkan SDK glslc)
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(COMPILED_SHADER_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")

set(SHADERS
  "${SHADER_DIR}/triangle.vert"
  "${SHADER_DIR}/triangle.frag"
)

# Try to locate glslc (Vulkan SDK)
find_program(GLSLC glslc
  HINTS
    "$ENV{VULKAN_SDK}/Bin"
    "$ENV{VULKAN_SDK}/Bin64"
)

if(GLSLC)
  foreach(SRC ${SHADERS})
    get_filename_component(FNAME "${SRC}" NAME)
    set(SPV "${COMPILED_SHADER_DIR}/${FNAME}.spv")
    add_custom_command(
      OUTPUT "${SPV}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${COMPILED_SHADER_DIR}"
      COMMAND "${GLSLC}" -o "${SPV}" "${SRC}"
      MAIN_DEPENDENCY "${SRC}"
      VERBATIM
    )
    list(APPEND SPV_SHADERS "${SPV}")
  endforeach()

  add_custom_target(shaders_spv ALL DEPENDS ${SPV_SHADERS})
  add_dependencies(luster_sandbox shaders_spv)
else()
  message(WARNING "glslc not found. Vulkan shaders will not be compiled. Ensure Vulkan SDK is installed and glslc is in PATH.")
endif()