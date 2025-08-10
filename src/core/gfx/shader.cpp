#include "core/gfx/shader.hpp"
#include <fstream>
#include <stdexcept>

namespace luster::gfx
{
    std::vector<char> Shader::readFileBinary(const std::string& path)
    {
        std::ifstream f(path, std::ios::ate | std::ios::binary);
        if (!f) throw std::runtime_error("Failed to open file: " + path);
        const size_t size = static_cast<size_t>(f.tellg());
        std::vector<char> buffer(size);
        f.seekg(0);
        if (!f.read(buffer.data(), buffer.size()))
        {
            throw std::runtime_error("Failed to read file: " + path);
        }
        return buffer;
    }

    static const char* vk_err(VkResult r)
    {
        switch (r)
        {
        case VK_SUCCESS: return "VK_SUCCESS";
        default: return "VK_ERROR";
        }
    }

    VkShaderModule Shader::createModule(VkDevice device, const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = code.size();
        ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule mod = VK_NULL_HANDLE;
        VkResult r = vkCreateShaderModule(device, &ci, nullptr, &mod);
        if (r != VK_SUCCESS) throw std::runtime_error(std::string("vkCreateShaderModule failed: ") + vk_err(r));
        return mod;
    }
}


