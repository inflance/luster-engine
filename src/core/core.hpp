// Common core includes and forward declarations
#pragma once

// GLM: use Vulkan depth range [0,1]
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif

#include <spdlog/spdlog.h>

#include <vulkan/vulkan.h>

// SDL3 core and Vulkan interop
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif
