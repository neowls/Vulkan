#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include "VulkanUtils.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Framebuffer.h"
#include "GraphicsPipeline.h"
#include "CommandBuffer.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

using namespace VulkanUtils;

#define WIDTH 1280
#define HEIGHT 720

#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
#endif

GLFWwindow* window = VK_NULL_HANDLE;
VkSurfaceKHR g_Surface = VK_NULL_HANDLE;

VkSemaphore imageAvailableSemaphore;
VkSemaphore renderFinishedSemaphore;
VkFence inFlightFence;

static ImGui_ImplVulkanH_Window  g_MainWindowData;

void initWindow();
void mainLoop();
void cleanupAll();
void drawFrame();

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
	wd->Surface = surface;

	// Check for WSI support
	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
	if (res != VK_TRUE)
	{
		fprintf(stderr, "Error no WSI support on physical device 0\n");
		exit(-1);
	}

	// Select Surface Format
	const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

	// Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
	wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
	//printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

	// Create SwapChain, RenderPass, Framebuffer, etc.
	IM_ASSERT(g_MinImageCount >= 2);
	ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

void initWindow()
{
	if (!glfwInit())
	{
		throw std::runtime_error("GLFW �ʱ�ȭ ����.");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(WIDTH, HEIGHT, "ImGui + Vulkan + GLFW", nullptr, nullptr);
	if (!window)
	{
		throw std::runtime_error("������ ���� ����");
	}
}

// ���� ����
void mainLoop()
{
	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		drawFrame();
	}
}

void createSyncObjects()
{
	VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	if (vkCreateSemaphore(g_Device, &semInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(g_Device, &semInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS)
		throw std::runtime_error("�������� ���� ����");

	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(g_Device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
		throw std::runtime_error("�潺 ���� ����");
}

bool createInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan + GLFW + ImGui";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// GLFW�� �ʿ��� extension �޾ƿ���
	uint32_t extCount = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extCount);
	createInfo.enabledExtensionCount = extCount;
	createInfo.ppEnabledExtensionNames = extensions;

	if (vkCreateInstance(&createInfo, g_Allocator, &g_Instance) != VK_SUCCESS)
	{
		std::cerr << "Vulkan �ν��Ͻ� ���� ����.\n";
		return false;
	}
	return true;
}

void initVulkan(GLFWwindow* window)
{
	// Vulkan �ν��Ͻ� ����
	if (!createInstance())
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}

	// ����� �޽���(Messenger) ���� (�ɼ�)
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	VulkanUtils::populateDebugMessengerCreateInfo(debugCreateInfo);
	VulkanUtils::CreateDebugUtilsMessengerEXT(g_Instance, &debugCreateInfo, nullptr, &debugMessenger);


	// ���� ����̽� ����
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(g_Instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		std::cerr << "Vulkan ���� GPU ����.\n";
		vkDestroyInstance(g_Instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(g_Instance, &deviceCount, physicalDevices.data());
	g_PhysicalDevice = physicalDevices[0]; // ù��° GPU ���


	// ť �йи� Ž��
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &queueFamilyCount, queueFamilies.data());
	g_QueueFamily = -1;
	for (uint32_t i = 0; i < queueFamilyCount; ++i)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			g_QueueFamily = i;
			break;
		}
	}

	if (g_QueueFamily == -1)
	{
		std::cerr << "�׷��� ť�� �����ϴ� �йи��� ����.\n";
		vkDestroyInstance(g_Instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}

	const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	// �� ����̽� �� ť ����
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = g_QueueFamily;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

	if (vkCreateDevice(g_PhysicalDevice, &deviceCreateInfo, g_Allocator, &g_Device) != VK_SUCCESS)
	{
		std::cerr << "�� ����̽� ���� ����.\n";
		return;
	}

	vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);

	// Ŀ�ǵ� Ǯ ����
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = g_QueueFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if (vkCreateCommandPool(g_Device, &poolInfo, nullptr, &g_CommandPool) != VK_SUCCESS)
		throw std::runtime_error("vkCreateCommandPool ����.");

	// Vulkan �����̽� ����
	if (glfwCreateWindowSurface(g_Instance, window, g_Allocator, &g_Surface) != VK_SUCCESS)
	{
		std::cerr << "Vulkan �����̽� ���� ����.\n";
		vkDestroyInstance(g_Instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}
}

void InitImGui(GLFWwindow* window)
{
	//	ImGui Context ��Ÿ��
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("C:/NotoSansKR-Regular.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesKorean());

	//	GLFW_IMGUI ����
	ImGui_ImplGlfw_InitForVulkan(window, true);

	// ��ũ���� Ǯ �غ�
	VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	if (vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool) != VK_SUCCESS)
		throw std::runtime_error("��ũ���� Ǯ ���� ����.");

	//	ImGui Vulkan �ʱ�ȭ
	ImGui_ImplVulkan_InitInfo init_info = {};
	init_info.Instance = g_Instance;
	init_info.PhysicalDevice = g_PhysicalDevice;
	init_info.Device = g_Device;
	init_info.QueueFamily = g_QueueFamily;
	init_info.Queue = g_Queue;
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = g_DescriptorPool;
	init_info.Subpass = 0;
	init_info.MinImageCount = 2;
	init_info.ImageCount = 2;
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = g_Allocator;
	init_info.CheckVkResultFn = nullptr;
	init_info.RenderPass = RenderPass::renderPass;

	ImGui_ImplVulkan_Init(&init_info);


}

//	��ü �ڿ� ����
void cleanupAll()
{
	vkDeviceWaitIdle(g_Device);
	CommandBuffer::cleanup(g_Device, g_CommandPool);
	Framebuffer::cleanup(g_Device);
	RenderPass::cleanup(g_Device);
	Swapchain::cleanup(g_Device);

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	ImGui::DestroyContext();

	vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
	vkDestroyCommandPool(g_Device, g_CommandPool, g_Allocator);
	vkDestroyDevice(g_Device, g_Allocator);
	vkDestroySurfaceKHR(g_Instance, g_Surface, g_Allocator);
	if (debugMessenger != VK_NULL_HANDLE) DestroyDebugUtilsMessengerEXT(g_Instance, debugMessenger, g_Allocator);
	vkDestroyInstance(g_Instance, g_Allocator);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void drawFrame()
{
	static uint32_t imageIndex = 0;

	vkWaitForFences(g_Device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
	vkResetFences(g_Device, 1, &inFlightFence);

	VkResult result = vkAcquireNextImageKHR(g_Device, Swapchain::swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// ����ü�� ������ �� ��
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to acquire swapchain image.");
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("ImGui + Vulkan");
	ImGui::Text("Success");
	ImGui::Text("�ѱ� �׽�Ʈ");
	ImGui::End();

	ImGui::Render();

	VkCommandBuffer cmd = CommandBuffer::commandBuffers[imageIndex];
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// 3. RenderPass ����
	VkClearValue clearValue = { { {0.15f, 0.15f, 0.15f, 1.0f} } };
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = RenderPass::renderPass;
	renderPassInfo.framebuffer = Framebuffer::framebuffers[imageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = Swapchain::extent;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	// ImGui DrawData�� ������ ����
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);


	vkCmdEndRenderPass(cmd);
	vkEndCommandBuffer(cmd);

	// ť ���� (�������� ����ȭ)
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

	if (vkQueueSubmit(g_Queue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit draw command buffer");
	}

	//	���������̼�
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &Swapchain::swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(g_Queue, &presentInfo);

	// VK_ERROR_OUT_OF_DATE_KHR (������ ũ�� ���� ��) ó��
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		// swapchain recreate �ʿ�
		// recreateSwapchain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to present swapchain image!");
	}
}

int main()
{
	try
	{
		initWindow();

		// Vulkan ���ҽ� �� ImGui �ʱ�ȭ
		initVulkan(window);
		createSyncObjects();

		Swapchain::setup(window, g_PhysicalDevice, g_Device, g_Surface, g_QueueFamily);
		RenderPass::setup(g_Device, Swapchain::imageFormat);
		Framebuffer::setup(g_Device, Swapchain::extent, RenderPass::renderPass, Swapchain::imageViews);
		CommandBuffer::create(g_Device, g_CommandPool, Framebuffer::framebuffers.size());

		ImGui_ImplVulkan_LoadFunctions(
			VK_API_VERSION_1_0,
			[](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
				return vkGetInstanceProcAddr(
					static_cast<VkInstance>(user_data),
					function_name
				);
			},
			g_Instance
		);

		InitImGui(window);
		mainLoop();
		cleanupAll();
		return 0;
	}
	catch(const std::exception& e)
	{
		std::cerr << "���� �߻� : " << e.what() << std::endl;
		return -1;
	}
}