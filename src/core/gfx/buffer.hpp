#pragma once

#include "core/core.hpp"
#include <cstdint>

namespace luster::gfx
{
    class Device;

    struct BufferCreateInfo
    {
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    };

    class Buffer
    {
    public:
        Buffer() = default;
        ~Buffer() = default;

        void create(const Device& device, const BufferCreateInfo& info);
        void cleanup(const Device& device);

        void* map(const Device& device);
        void unmap(const Device& device);

        VkBuffer handle() const { return buffer_; }
        VkDeviceSize size() const { return size_; }

    private:
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;
        VkDeviceSize size_ = 0;
    };
}


