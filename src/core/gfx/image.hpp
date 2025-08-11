#pragma once

#include "core/core.hpp"

namespace luster::gfx
{
	class Device;

	struct ImageCreateInfo
	{
		uint32_t width = 0;
		uint32_t height = 0;
		VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
		VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	};

	class Image
	{
	public:
		Image() = default;
		~Image() = default;

		void create(const Device& device, const ImageCreateInfo& info);
		void cleanup(const Device& device);

		VkImage image() const { return image_; }
		VkImageView view() const { return view_; }
		VkFormat format() const { return format_; }
		uint32_t width() const { return width_; }
		uint32_t height() const { return height_; }

	private:
		VkImage image_ = VK_NULL_HANDLE;
		VkDeviceMemory memory_ = VK_NULL_HANDLE;
		VkImageView view_ = VK_NULL_HANDLE;
		VkFormat format_ = VK_FORMAT_UNDEFINED;
		uint32_t width_ = 0;
		uint32_t height_ = 0;
	};
}
