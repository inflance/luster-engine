#include "core/gfx/swapchain.hpp"
#include "core/gfx/device.hpp"
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace luster::gfx
{
    struct SwapchainSupport
    {
        VkSurfaceCapabilitiesKHR caps{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    static SwapchainSupport querySwapchainSupport(VkPhysicalDevice gpu, VkSurfaceKHR surface)
    {
        SwapchainSupport sup{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &sup.caps);
        uint32_t count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
        sup.formats.resize(count);
        if (count) vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, sup.formats.data());
        count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
        sup.presentModes.resize(count);
        if (count) vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, sup.presentModes.data());
        return sup;
    }

    VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats,
                                                      VkFormat preferredFormat,
                                                      VkColorSpaceKHR preferredColorSpace)
    {
        if (formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
            return {preferredFormat, preferredColorSpace};
        for (const auto& f : formats)
        {
            if (f.format == preferredFormat && f.colorSpace == preferredColorSpace) return f;
        }
        return formats[0];
    }

    VkPresentModeKHR Swapchain::choosePresentMode(const std::vector<VkPresentModeKHR>& modes)
    {
        for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::chooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* window)
    {
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) return caps.currentExtent;
        int w = 0, h = 0; SDL_GetWindowSize(window, &w, &h);
        VkExtent2D e{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
        e.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, e.width));
        e.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, e.height));
        return e;
    }

    void Swapchain::create(const Device& device, SDL_Window* window, const SwapchainCreateInfo& info)
    {
        auto sup = querySwapchainSupport(device.physical(), device.surface());
        const VkSurfaceFormatKHR fmt = chooseSurfaceFormat(sup.formats, info.preferredFormat, info.preferredColorSpace);
        VkPresentModeKHR present = info.preferredPresentMode;
        bool supported = false;
        for (auto m : sup.presentModes) if (m == present) { supported = true; break; }
        if (!supported)
        {
            // fallback：MAILBOX 优先，否则 FIFO
            present = choosePresentMode(sup.presentModes);
        }
        const VkExtent2D extent = chooseExtent(sup.caps, window);

        uint32_t imageCount = sup.caps.minImageCount + 1;
        if (sup.caps.maxImageCount > 0 && imageCount > sup.caps.maxImageCount) imageCount = sup.caps.maxImageCount;

        VkSwapchainCreateInfoKHR sci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        sci.surface = device.surface();
        sci.minImageCount = imageCount;
        sci.imageFormat = fmt.format;
        sci.imageColorSpace = fmt.colorSpace;
        sci.imageExtent = extent;
        sci.imageArrayLayers = 1;
        sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t indices[] = {device.gfxQueueFamily(), device.presentQueueFamily()};
        if (device.gfxQueueFamily() != device.presentQueueFamily())
        {
            sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            sci.queueFamilyIndexCount = 2;
            sci.pQueueFamilyIndices = indices;
        }
        else
        {
            sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        sci.preTransform = sup.caps.currentTransform;
        sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        sci.presentMode = present;
        sci.clipped = VK_TRUE;

        VkResult r = vkCreateSwapchainKHR(device.logical(), &sci, nullptr, &swapchain_);
        if (r != VK_SUCCESS) throw std::runtime_error("vkCreateSwapchainKHR failed");

        swapFormat_ = fmt.format;
        swapColorSpace_ = fmt.colorSpace;
        swapExtent_ = extent;

        vkGetSwapchainImagesKHR(device.logical(), swapchain_, &imageCount, nullptr);
        swapImages_.resize(imageCount);
        vkGetSwapchainImagesKHR(device.logical(), swapchain_, &imageCount, swapImages_.data());

        swapImageViews_.resize(swapImages_.size());
        for (size_t i = 0; i < swapImages_.size(); ++i)
        {
            VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            ci.image = swapImages_[i];
            ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ci.format = swapFormat_;
            ci.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                             VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
            ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ci.subresourceRange.levelCount = 1;
            ci.subresourceRange.layerCount = 1;
            VkResult rv = vkCreateImageView(device.logical(), &ci, nullptr, &swapImageViews_[i]);
            if (rv != VK_SUCCESS) throw std::runtime_error("vkCreateImageView failed");
        }
    }

    void Swapchain::recreate(const Device& device, SDL_Window* window, const SwapchainCreateInfo& info)
    {
        cleanup(device);
        create(device, window, info);
    }

    void Swapchain::cleanup(const Device& device)
    {
        for (auto iv : swapImageViews_) if (iv) vkDestroyImageView(device.logical(), iv, nullptr);
        swapImageViews_.clear();
        swapImages_.clear();
        if (swapchain_) vkDestroySwapchainKHR(device.logical(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}


