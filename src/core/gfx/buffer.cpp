// Single, clean implementation (removed duplicate definitions)
#include "core/gfx/buffer.hpp"
#include "core/gfx/device.hpp"
#include <stdexcept>
#include <cstring>

namespace luster::gfx
{
    static uint32_t findMemoryType(VkPhysicalDevice gpu, uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties{};
        vkGetPhysicalDeviceMemoryProperties(gpu, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        throw std::runtime_error("No suitable memory type found");
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
        hostVisible_ = (info.properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    }

    void Buffer::cleanup(const Device& device)
    {
        if (memory_) { vkFreeMemory(device.logical(), memory_, nullptr); memory_ = VK_NULL_HANDLE; }
        if (buffer_) { vkDestroyBuffer(device.logical(), buffer_, nullptr); buffer_ = VK_NULL_HANDLE; }
        size_ = 0;
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

    void Buffer::upload(const Device& device, const void* src, VkDeviceSize size)
    {
        if (size == 0 || !src) return;
        // Try direct map
        if (hostVisible_)
        {
            void* dst = nullptr;
            if (vkMapMemory(device.logical(), memory_, 0, size_, 0, &dst) != VK_SUCCESS)
                throw std::runtime_error("vkMapMemory failed for host-visible buffer");
            std::memcpy(dst, src, static_cast<size_t>(std::min(size_, size)));
            vkUnmapMemory(device.logical(), memory_);
            return;
        }

        // Fallback staging (DEVICE_LOCAL memory)
        Buffer staging;
        BufferCreateInfo ci{};
        ci.size = size;
        ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        ci.properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        staging.create(device, ci);
        void* mapped = staging.map(device);
        std::memcpy(mapped, src, static_cast<size_t>(size));
        staging.unmap(device);

        // Ensure dst buffer has TRANSFER_DST usage
        // Copy via immediate submit
        device.submitImmediate([&](VkCommandBuffer cmd) {
            VkBufferCopy region{}; region.size = size;
            vkCmdCopyBuffer(cmd, staging.handle(), buffer_, 1, &region);
        });
        staging.cleanup(device);
    }
}


