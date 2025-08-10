#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
    class Device;

    class DescriptorSetLayout
    {
    public:
        DescriptorSetLayout() = default;
        ~DescriptorSetLayout() = default;

        void create(const Device& device, const VkDescriptorSetLayoutBinding* bindings, uint32_t count);
        void cleanup(const Device& device);
        VkDescriptorSetLayout handle() const { return layout_; }

    private:
        VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
    };

    class DescriptorPool
    {
    public:
        void create(const Device& device, const VkDescriptorPoolSize* sizes, uint32_t sizeCount, uint32_t maxSets);
        void cleanup(const Device& device);
        VkDescriptorPool handle() const { return pool_; }

    private:
        VkDescriptorPool pool_ = VK_NULL_HANDLE;
    };

    class DescriptorSet
    {
    public:
        void allocate(const Device& device, const DescriptorPool& pool, const DescriptorSetLayout& layout);
        void updateUniformBuffer(const Device& device, uint32_t binding, VkBuffer buffer, VkDeviceSize range);
        VkDescriptorSet handle() const { return set_; }
    private:
        VkDescriptorSet set_ = VK_NULL_HANDLE;
    };
}



