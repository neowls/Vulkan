#pragma once
#pragma once
#include <vulkan/vulkan.h>
#include <vector>


namespace CommandBuffer 
{

    extern std::vector<VkCommandBuffer> commandBuffers;

    void create(VkDevice device, VkCommandPool commandPool, size_t count);
    void recordAll(VkDevice device, VkRenderPass renderPass, const std::vector<VkFramebuffer>& framebuffers, VkExtent2D extent, VkPipeline graphicsPipeline);
    void cleanup(VkDevice device, VkCommandPool commandPool);

}