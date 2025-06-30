#pragma once
#include <vulkan/vulkan.h>
#include <vector>

namespace Framebuffer
{
    extern std::vector<VkFramebuffer> framebuffers;

    void setup(VkDevice device, VkExtent2D extent, VkRenderPass renderPass, const std::vector<VkImageView>& imageViews);
    void cleanup(VkDevice device);
}