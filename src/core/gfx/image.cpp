#include "core/gfx/image.hpp"
#include "core/gfx/device.hpp"
#include <stdexcept>

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

	void Image::create(const Device& device, const ImageCreateInfo& info)
	{
		cleanup(device);
		width_ = info.width;
		height_ = info.height;
		format_ = info.format;

		VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		ici.imageType = VK_IMAGE_TYPE_2D;
		ici.extent = {info.width, info.height, 1};
		ici.mipLevels = 1;
		ici.arrayLayers = 1;
		ici.format = info.format;
		ici.tiling = info.tiling;
		ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		ici.usage = info.usage;
		ici.samples = VK_SAMPLE_COUNT_1_BIT;
		ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VkResult r = vkCreateImage(device.logical(), &ici, nullptr, &image_);
		if (r != VK_SUCCESS) throw std::runtime_error("vkCreateImage failed");

		VkMemoryRequirements req{};
		vkGetImageMemoryRequirements(device.logical(), image_, &req);

		VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
		ai.allocationSize = req.size;
		ai.memoryTypeIndex = findMemoryType(device.physical(), req.memoryTypeBits, info.properties);
		r = vkAllocateMemory(device.logical(), &ai, nullptr, &memory_);
		if (r != VK_SUCCESS) throw std::runtime_error("vkAllocateMemory failed");
		vkBindImageMemory(device.logical(), image_, memory_, 0);

		VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		vi.image = image_;
		vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
		vi.format = info.format;
		vi.subresourceRange.aspectMask = info.aspect;
		vi.subresourceRange.baseMipLevel = 0;
		vi.subresourceRange.levelCount = 1;
		vi.subresourceRange.baseArrayLayer = 0;
		vi.subresourceRange.layerCount = 1;
		r = vkCreateImageView(device.logical(), &vi, nullptr, &view_);
		if (r != VK_SUCCESS) throw std::runtime_error("vkCreateImageView failed");
	}

	void Image::cleanup(const Device& device)
	{
		if (view_) vkDestroyImageView(device.logical(), view_, nullptr);
		if (memory_) vkFreeMemory(device.logical(), memory_, nullptr);
		if (image_) vkDestroyImage(device.logical(), image_, nullptr);
		image_ = VK_NULL_HANDLE;
		memory_ = VK_NULL_HANDLE;
		view_ = VK_NULL_HANDLE;
		width_ = height_ = 0;
		format_ = VK_FORMAT_UNDEFINED;
	}
}
