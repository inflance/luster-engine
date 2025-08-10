#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
    class Device;

    struct BufferCreateInfo
    {
        VkDeviceSize size = 0;
        VkBufferUsageFlags usage = 0;
        VkMemoryPropertyFlags properties = 0;
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

        // Convenience: upload data via staging if memory is DEVICE_LOCAL, else direct map
        void upload(const Device& device, const void* data, VkDeviceSize size);

        VkBuffer handle() const { return buffer_; }
        VkDeviceSize size() const { return size_; }

    private:
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VkDeviceMemory memory_ = VK_NULL_HANDLE;
        VkDeviceSize size_ = 0;
    };
}


