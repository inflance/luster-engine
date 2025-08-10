#include "core/gfx/buffer.hpp"
#include "core/gfx/device.hpp"
#include <stdexcept>
#include <vector>

namespace luster::gfx
{
    static uint32_t findMemoryType(VkPhysicalDevice gpu, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(gpu, &memProps);
        for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("Failed to find suitable memory type");
    }

    void Buffer::create(const Device& device, const BufferCreateInfo& info)
    {
        cleanup(device);
        size_ = info.size;

        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = info.size;
        bi.usage = info.usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkResult r = vkCreateBuffer(device.logical(), &bi, nullptr, &buffer_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateBuffer failed");

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device.logical(), buffer_, &req);

        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = findMemoryType(device.physical(), req.memoryTypeBits, info.properties);
        r = vkAllocateMemory(device.logical(), &ai, nullptr, &memory_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkAllocateMemory failed");

        vkBindBufferMemory(device.logical(), buffer_, memory_, 0);
    }

    void Buffer::cleanup(const Device& device)
    {
        if (memory_) vkFreeMemory(device.logical(), memory_, nullptr);
        if (buffer_) vkDestroyBuffer(device.logical(), buffer_, nullptr);
        buffer_ = VK_NULL_HANDLE; memory_ = VK_NULL_HANDLE; size_ = 0;
    }

    void* Buffer::map(const Device& device)
    {
        void* data = nullptr;
        VkResult r = vkMapMemory(device.logical(), memory_, 0, size_, 0, &data);
        if (r != VK_SUCCESS) throw std::runtime_error("vkMapMemory failed");
        return data;
    }

    void Buffer::unmap(const Device& device)
    {
        vkUnmapMemory(device.logical(), memory_);
    }
}


