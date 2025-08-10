#include "core/gfx/pipeline.hpp"
#include "core/gfx/device.hpp"
#include "core/gfx/render_pass.hpp"
#include "core/gfx/shader.hpp"
#include <stdexcept>

namespace luster::gfx
{
    void Pipeline::create(const Device& device, const RenderPass& rp, const PipelineCreateInfo& info)
    {
        auto vsCode = Shader::readFileBinary(info.vsSpvPath);
        auto fsCode = Shader::readFileBinary(info.fsSpvPath);
        VkShaderModule vs = Shader::createModule(device.logical(), vsCode);
        VkShaderModule fs = Shader::createModule(device.logical(), fsCode);

        VkPipelineShaderStageCreateInfo vsStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vsStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vsStage.module = vs;
        vsStage.pName = "main";

        VkPipelineShaderStageCreateInfo fsStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fsStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fsStage.module = fs;
        fsStage.pName = "main";

        VkPipelineShaderStageCreateInfo stages[] = {vsStage, fsStage};

        VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

        VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport vp{};
        vp.width = static_cast<float>(info.viewportExtent.width);
        vp.height = static_cast<float>(info.viewportExtent.height);
        vp.minDepth = 0.f; vp.maxDepth = 1.f;

        VkRect2D sc{};
        sc.extent = info.viewportExtent;

        VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        vpState.viewportCount = 1; vpState.pViewports = &vp;
        vpState.scissorCount = 1;  vpState.pScissors = &sc;

        VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rs.polygonMode = VK_POLYGON_MODE_FILL;
        rs.cullMode = VK_CULL_MODE_BACK_BIT;
        rs.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rs.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        cb.attachmentCount = 1; cb.pAttachments = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        VkResult r = vkCreatePipelineLayout(device.logical(), &pl, nullptr, &pipelineLayout_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreatePipelineLayout failed");

        VkGraphicsPipelineCreateInfo pci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pci.stageCount = 2; pci.pStages = stages;
        pci.pVertexInputState = &vi;
        pci.pInputAssemblyState = &ia;
        pci.pViewportState = &vpState;
        pci.pRasterizationState = &rs;
        pci.pMultisampleState = &ms;
        pci.pDepthStencilState = nullptr;
        pci.pColorBlendState = &cb;
        pci.pDynamicState = nullptr;
        pci.layout = pipelineLayout_;
        pci.renderPass = rp.handle();
        pci.subpass = 0;

        r = vkCreateGraphicsPipelines(device.logical(), VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateGraphicsPipelines failed");

        vkDestroyShaderModule(device.logical(), fs, nullptr);
        vkDestroyShaderModule(device.logical(), vs, nullptr);
    }

    void Pipeline::cleanup(const Device& device)
    {
        if (pipeline_) vkDestroyPipeline(device.logical(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
        if (pipelineLayout_) vkDestroyPipelineLayout(device.logical(), pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
}


