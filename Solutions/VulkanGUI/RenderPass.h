#pragma once
#include <vulkan/vulkan.h>

namespace RenderPass
{
	extern VkRenderPass renderPass;


    void setup(VkDevice device, VkFormat imageFormat);
    void cleanup(VkDevice device);
}