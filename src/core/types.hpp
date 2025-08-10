#pragma once

#include <cstdint>

namespace luster
{
    struct Extent2D
    {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct ColorRGBA
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };
}


