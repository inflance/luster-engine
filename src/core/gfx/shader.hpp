#pragma once

#include "core/core.hpp"
#include <string>
#include <vector>

namespace luster::gfx
{
    class Shader
    {
    public:
        static std::vector<char> readFileBinary(const std::string& path);
        static VkShaderModule createModule(VkDevice device, const std::vector<char>& code);
    };
}


