#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace GraphicsPipeline
{
	extern VkPipelineLayout g_PipelineLayout;
	extern VkPipeline g_Pipeline;

	void setup(VkDevice device, VkExtent2D extent, VkRenderPass renderPass, const std::string& vectShaderPath, const std::string& fragShaderPath);
	void cleanup(VkDevice device);
}