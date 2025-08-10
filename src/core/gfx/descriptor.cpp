#include "core/gfx/descriptor.hpp"
#include "core/gfx/device.hpp"
#include <stdexcept>

namespace luster::gfx
{
    void DescriptorSetLayout::create(const Device& device, const VkDescriptorSetLayoutBinding* bindings, uint32_t count)
    {
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = count; ci.pBindings = bindings;
        VkResult r = vkCreateDescriptorSetLayout(device.logical(), &ci, nullptr, &layout_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateDescriptorSetLayout failed");
    }

    void DescriptorSetLayout::cleanup(const Device& device)
    {
        if (layout_) vkDestroyDescriptorSetLayout(device.logical(), layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }

    void DescriptorPool::create(const Device& device, const VkDescriptorPoolSize* sizes, uint32_t sizeCount, uint32_t maxSets)
    {
        VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        ci.poolSizeCount = sizeCount; ci.pPoolSizes = sizes; ci.maxSets = maxSets;
        VkResult r = vkCreateDescriptorPool(device.logical(), &ci, nullptr, &pool_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateDescriptorPool failed");
    }

    void DescriptorPool::cleanup(const Device& device)
    {
        if (pool_) vkDestroyDescriptorPool(device.logical(), pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }

    void DescriptorSet::allocate(const Device& device, const DescriptorPool& pool, const DescriptorSetLayout& layout)
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        VkDescriptorSetLayout l = layout.handle();
        ai.descriptorPool = pool.handle(); ai.descriptorSetCount = 1; ai.pSetLayouts = &l;
        VkResult r = vkAllocateDescriptorSets(device.logical(), &ai, &set_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkAllocateDescriptorSets failed");
    }

    void DescriptorSet::updateUniformBuffer(const Device& device, uint32_t binding, VkBuffer buffer, VkDeviceSize range)
    {
        VkDescriptorBufferInfo dbi{}; dbi.buffer = buffer; dbi.offset = 0; dbi.range = range;
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set_;
        write.dstBinding = binding;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(device.logical(), 1, &write, 0, nullptr);
    }
}



