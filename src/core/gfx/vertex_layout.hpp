#pragma once

#include "core/core.hpp"
#include <vector>

namespace luster::gfx
{
    class VertexLayout
    {
    public:
        VertexLayout() = default;

        void setBinding(uint32_t binding, uint32_t stride, VkVertexInputRate rate = VK_VERTEX_INPUT_RATE_VERTEX)
        {
            binding_.binding = binding;
            binding_.stride = stride;
            binding_.inputRate = rate;
            hasBinding_ = true;
        }

        void addAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset)
        {
            VkVertexInputAttributeDescription a{};
            a.location = location;
            a.binding = binding;
            a.format = format;
            a.offset = offset;
            attributes_.push_back(a);
        }

        bool hasBinding() const { return hasBinding_; }
        const VkVertexInputBindingDescription* binding() const { return hasBinding_ ? &binding_ : nullptr; }
        const VkVertexInputAttributeDescription* attributesData() const { return attributes_.empty() ? nullptr : attributes_.data(); }
        uint32_t attributeCount() const { return static_cast<uint32_t>(attributes_.size()); }

    private:
        VkVertexInputBindingDescription binding_{};
        bool hasBinding_ = false;
        std::vector<VkVertexInputAttributeDescription> attributes_{};
    };
}



