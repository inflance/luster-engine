#pragma once

#include "core/core.hpp"
#include <string>

namespace luster::gfx
{
    class Device;
    class RenderPass;
    class VertexLayout;

    struct PipelineCreateInfo
    {
        std::string vsSpvPath;
        std::string fsSpvPath;
        VkExtent2D viewportExtent{};
        VkBool32 enableDepthTest = VK_TRUE;
        VkBool32 enableDepthWrite = VK_TRUE;
        // 顶点输入：支持最多 1 个 binding（当前需求）
        const VkVertexInputBindingDescription* vertexBinding = nullptr;
        uint32_t vertexBindingCount = 0; // 0 or 1
        const VkVertexInputAttributeDescription* vertexAttributes = nullptr;
        uint32_t vertexAttributeCount = 0;
        // 可选更高层：直接传 VertexLayout（优先于上面的裸指针字段）
        const VertexLayout* vertexLayout = nullptr;
        // 可选：描述符布局（如 UBO）
        const VkDescriptorSetLayout* setLayouts = nullptr;
        uint32_t setLayoutCount = 0;
    };

    class Pipeline
    {
    public:
        Pipeline() = default;
        ~Pipeline() = default;

        void create(const Device& device, const RenderPass& rp, const PipelineCreateInfo& info);
        void cleanup(const Device& device);

        VkPipelineLayout layout() const { return pipelineLayout_; }
        VkPipeline handle() const { return pipeline_; }

    private:
        VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
    };
}


