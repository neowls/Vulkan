#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace Swapchain
{
	extern VkSwapchainKHR swapchain;
	extern std::vector<VkImage> images;
	extern std::vector<VkImageView> imageViews;
	extern VkFormat imageFormat;
	extern VkExtent2D extent;

	void setup(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t graphicsQueueFamily);
	void cleanup(VkDevice device);
}