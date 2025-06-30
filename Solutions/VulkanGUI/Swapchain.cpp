#include "Swapchain.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>



namespace Swapchain
{
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	VkFormat imageFormat;
	VkExtent2D extent;

	void Swapchain::setup(GLFWwindow* window, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, uint32_t graphicsQueueFamily)
	{
		// Surface Capbilities 조회
		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

		// Surface Formats 조회
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

		//	Present Modes 조회
		uint32_t presentModeCount = 0;
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
		std::vector<VkPresentModeKHR> presentModes(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

		//	Surface Format 선택
		VkSurfaceFormatKHR surfaceFormat = formats[0];
		for (const auto& f : formats)
		{
			if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				surfaceFormat = f;
				break;
			}
		}

		//	Present Mode 선택
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& pm : presentModes)
		{
			if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				presentMode = pm;
				break;
			}
		}

		//	Extent 선택
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D swapExtent =
		{
			static_cast<uint32_t>(std::clamp(width, (int)capabilities.minImageExtent.width, (int)capabilities.maxImageExtent.width)),
			static_cast<uint32_t>(std::clamp(height, (int)capabilities.minImageExtent.height, (int)capabilities.maxImageExtent.height))
		};

		//	이미지 개수
		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
			imageCount = capabilities.maxImageCount;

		// SwapchainCreateInfo 세팅
		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = swapExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		uint32_t queueFamilyIndices[] = { graphicsQueueFamily };
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 1;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
		createInfo.preTransform = capabilities.currentTransform;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		VkResult res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);

		if (res != VK_SUCCESS)
		{
			std::cerr << "스왑체인 생성 실패. VkResult = " << res << std::endl;
			throw std::runtime_error("스왑체인 생성 실패.");
		}
			

		//	이미지 얻기
		uint32_t imgCount = 0;
		vkGetSwapchainImagesKHR(device, swapchain, &imgCount, nullptr);
		images.resize(imgCount);
		vkGetSwapchainImagesKHR(device, swapchain, &imgCount, images.data());

		//	이미지 뷰 생성
		imageViews.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; ++i)
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = images[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = surfaceFormat.format;
			viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = 1;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]) != VK_SUCCESS)
				throw std::runtime_error("이미지 뷰 생성 실패");
		}

		//	형식/크기 기록
		imageFormat = surfaceFormat.format;
		extent = swapExtent;
	}

	void Swapchain::cleanup(VkDevice device)
	{
		for (auto view : imageViews)
		{
			vkDestroyImageView(device, view, nullptr);
		}

		imageViews.clear();
		images.clear();

		if (swapchain != VK_NULL_HANDLE)
		{
			vkDestroySwapchainKHR(device, swapchain, nullptr);
		}

		swapchain = VK_NULL_HANDLE;
	}
}