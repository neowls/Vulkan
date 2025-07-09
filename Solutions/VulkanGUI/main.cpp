#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <sstream>
#include <string>
#include <mutex>
#include <map>
#include <random>
#include <exception>
#include "VulkanUtils.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Framebuffer.h"
#include "GraphicsPipeline.h"
#include "CommandBuffer.h"
#include "ChatUI.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#pragma comment(lib, "ws2_32.lib")

using namespace VulkanUtils;

#define WIDTH 1280
#define HEIGHT 720

#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;

#endif

struct ClientInfo
{
	SOCKET socket;
	std::string uuid;
	std::string nickname;
};


GLFWwindow* window = VK_NULL_HANDLE;
VkSurfaceKHR g_Surface = VK_NULL_HANDLE;
ImFont* g_FontKR = nullptr;

VkSemaphore imageAvailableSemaphore;
VkSemaphore renderFinishedSemaphore;
VkFence inFlightFence;

static ImGui_ImplVulkanH_Window  g_MainWindowData;
static std::thread serverThread;
static bool serverRunning = false;

std::vector<std::string> chatMessages;
std::map<SOCKET, ClientInfo> clients;
std::mutex chatMutex;
std::atomic<bool> running(true);

SOCKET sock = INVALID_SOCKET;


void initWindow();
void mainLoop();
void cleanupAll();
void drawFrame();

std::string GenerateRandomUUID()
{
	static std::mt19937_64 rng(std::random_device{}());
	static std::uniform_int_distribution<uint64_t> dist;

	std::stringstream ss;
	ss << std::hex;
	for (int i = 0; i < 2; ++i)
	{
		ss.width(8);
		ss.fill('0');
		ss << dist(rng);
	}
	return ss.str();
}

// 모든 클라이언트에게 메세지 브로드캐스트
void BroadcastMessage(const std::string& msg) 
{
	std::lock_guard<std::mutex> lock(chatMutex);
	for (const auto& [sock, info] : clients)
	{
		send(sock, msg.c_str(), (int)msg.size(), 0);
	}
}

//	서버 - 클라이언트 스레드
void Handle_Client(SOCKET client_sock)
{
	char buf[1024] = {0};
	//	닉네임 수신
	int n = recv(client_sock, buf, sizeof(buf) - 1, 0);
	if (n <= 0)
	{
		closesocket(client_sock);
		return;
	}

	buf[n] = 0;

	//	클라이언트 등록
	std::string nickname(buf);
	{
		std::lock_guard<std::mutex> lock(chatMutex);
		clients[client_sock] = { client_sock, GenerateRandomUUID(), nickname };
	}

	//	입장 메세지 브로드캐스트
	BroadcastMessage("[시스템] " + std::string(nickname) + "님이 입장하였습니다.");


	//	메세지 루프
	while (true)
	{
		n = recv(client_sock, buf, sizeof(buf) - 1, 0);
		if (n <= 0) break;
		buf[n] = 0;
		std::string text(buf);

		BroadcastMessage("[" + nickname + "] " + text);
	}
	
	//	연결 해제
	{
		std::lock_guard<std::mutex> lock(chatMutex);
		clients.erase(client_sock);
	}
	closesocket(client_sock);

	BroadcastMessage("[시스템] " + nickname + " 님이 퇴장하였습니다.");
}

void SocketReceiveThread(SOCKET sock)
{
	char buf[1024];
	while (running)
	{
		int n = recv(sock, buf, sizeof(buf) - 1, 0);
		if (n > 0)
		{
			buf[n] = 0;
			{
				std::lock_guard<std::mutex> lock(chatMutex);
				chatMessages.emplace_back(buf);
			}
		}
		else
		{
			ChatUI::ToLobby();
			running = false;
			break;
		}
	}
}

//	서버 스레드
void ChatServerThread()
{
	//	서버용 변수 선언
	WSADATA wsaData;
	SOCKET server_sock = INVALID_SOCKET;

	//	Winsock 초기화
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::lock_guard<std::mutex> lock(chatMutex);
		chatMessages.emplace_back(u8"[서버] WSAStartup 실패.");
		return;
	}

	//	소켓 생성
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == INVALID_SOCKET)
	{
		std::lock_guard<std::mutex> lock(chatMutex);
		chatMessages.emplace_back(u8"[서버] 소켓 생성 실패.");
		WSACleanup();
		return;
	}

	//	바인딩
	sockaddr_in serv_addr = {};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(8888);

	if (bind(server_sock, (SOCKADDR*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
	{
		std::lock_guard<std::mutex> lock(chatMutex);
		chatMessages.emplace_back(u8"[서버] 바인드 실패.(이미 사용중인 포트)");
		closesocket(server_sock);
		WSACleanup();
		return;
	}

	//	리슨
	if (listen(server_sock, 5) == SOCKET_ERROR)
	{
		std::lock_guard<std::mutex> lock(chatMutex);
		chatMessages.emplace_back(u8"[서버] 리슨 실패.");
		closesocket(server_sock);
		WSACleanup();
		return;
	}

	{
		std::lock_guard<std::mutex> lock(chatMutex);
		chatMessages.emplace_back("[서버] 채팅 서버 시작. 포트 8888");
	}

	//	연결 수락 및 데이터 처리 루프
	while (running)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(server_sock, &readfds);

		timeval timeout = { 0, 200 * 1000 }; // 200ms
		int ready = select(0, &readfds, NULL, NULL, &timeout);

		if (!running) break;
		if (ready > 0 && FD_ISSET(server_sock, &readfds))
		{
			SOCKET client_sock = accept(server_sock, NULL, NULL);
			if (client_sock != INVALID_SOCKET)
			{
				{
					std::thread(Handle_Client, client_sock).detach();
				}
			}
		}
	}

	// 클라이언트간 연결 종료
	for (const auto& client : clients)
	{
		closesocket(client.first);
	}

	// 서버 종료
	closesocket(server_sock);

	WSACleanup();
	{
		std::lock_guard<std::mutex> lock(chatMutex);
		chatMessages.emplace_back("[서버] 채팅 서버 종료.");
	}
}

bool ConnectToServer(const char* server_ip, const char* nickname) 
{
	// Winsock 초기화
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return false;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) return false;

	sockaddr_in serv_addr = {};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(8888);
	inet_pton(AF_INET, server_ip, &serv_addr.sin_addr);

	if (connect(sock, (SOCKADDR*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
		return false;

	// 로그인 메세지 전송
	std::string login_msg = u8"";
	login_msg += nickname;
	send(sock, login_msg.c_str(), (int)login_msg.size(), 0);

	return true;
}

void CleanupSocket() 
{
	running = false;
	if (sock != INVALID_SOCKET) closesocket(sock);
	WSACleanup();
}

ChatUI::Callbacks chatCallbacks
{
	//	방 생성
	[](const std::string& nickname)
	{
		if (!serverRunning)
		{
			serverThread = std::thread(ChatServerThread);
			serverThread.detach();
			serverRunning = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}

		// 자신의 서버의 접속
		if (!ConnectToServer("127.0.0.1", nickname.c_str()))
		{
			std::lock_guard<std::mutex> lock(chatMutex);
			chatMessages.emplace_back("[시스템] 서버 연결 실패.");
		}
		else
		{
			std::lock_guard<std::mutex> lock(chatMutex);
			chatMessages.emplace_back("[시스템] 방을 생성했습니다.");
			std::thread(SocketReceiveThread, sock).detach();
		}
	},
	//	방 참가
	[](const std::string& nickname, const std::string& ip)
	{
		if (!ConnectToServer(ip.c_str(), nickname.c_str()))
		{
			std::lock_guard<std::mutex> lock(chatMutex);
			chatMessages.emplace_back("[시스템] 서버 연결 실패.");
		}
		else
		{
			std::lock_guard<std::mutex> lock(chatMutex);
			chatMessages.emplace_back("[시스템] 방에 참가했습니다.");
			std::thread(SocketReceiveThread, sock).detach();
		}
	},
	//	메세지 전송
	[](const std::string& message)
	{
		if (sock != INVALID_SOCKET)
		{
			send(sock, message.c_str(), static_cast<int>(message.size()), 0);
		}
		// 채팅창에 바로 표시 (Echo)
		//std::lock_guard<std::mutex> lock(chatMutex);
		//chatMessages.push_back(ChatUI::nickname + " : " + message);
	}
};

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
		throw std::runtime_error("GLFW 초기화 실패.");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(WIDTH, HEIGHT, "ImGui + Vulkan + GLFW", nullptr, nullptr);
	if (!window)
	{
		throw std::runtime_error("윈도우 생성 실패");
	}
}

// 메인 루프
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
		throw std::runtime_error("세마포어 생성 실패");

	VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	if (vkCreateFence(g_Device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS)
		throw std::runtime_error("펜스 생성 실패");
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

	// GLFW가 필요한 extension 받아오기
	uint32_t extCount = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&extCount);
	createInfo.enabledExtensionCount = extCount;
	createInfo.ppEnabledExtensionNames = extensions;

	if (vkCreateInstance(&createInfo, g_Allocator, &g_Instance) != VK_SUCCESS)
	{
		std::cerr << "Vulkan 인스턴스 생성 실패.\n";
		return false;
	}
	return true;
}

void initVulkan(GLFWwindow* window)
{
	// Vulkan 인스턴스 생성
	if (!createInstance())
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}

	// 디버그 메시지(Messenger) 생성 (옵션)
	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
	VulkanUtils::populateDebugMessengerCreateInfo(debugCreateInfo);
	VulkanUtils::CreateDebugUtilsMessengerEXT(g_Instance, &debugCreateInfo, nullptr, &debugMessenger);


	// 물리 디바이스 선택
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(g_Instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		std::cerr << "Vulkan 지원 GPU 없음.\n";
		vkDestroyInstance(g_Instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(g_Instance, &deviceCount, physicalDevices.data());
	g_PhysicalDevice = physicalDevices[0]; // 첫번째 GPU 사용


	// 큐 패밀리 탐색
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
		std::cerr << "그래픽 큐를 지원하는 패밀리가 없음.\n";
		vkDestroyInstance(g_Instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}

	const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	// 논리 디바이스 및 큐 생성
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
		std::cerr << "논리 디바이스 생성 실패.\n";
		return;
	}

	vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);

	// 커맨드 풀 생성
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = g_QueueFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if (vkCreateCommandPool(g_Device, &poolInfo, nullptr, &g_CommandPool) != VK_SUCCESS)
		throw std::runtime_error("vkCreateCommandPool 실패.");

	// Vulkan 서페이스 생성
	if (glfwCreateWindowSurface(g_Instance, window, g_Allocator, &g_Surface) != VK_SUCCESS)
	{
		std::cerr << "Vulkan 서페이스 생성 실패.\n";
		vkDestroyInstance(g_Instance, nullptr);
		glfwDestroyWindow(window);
		glfwTerminate();
		return;
	}
}

void InitImGui(GLFWwindow* window)
{
	//	ImGui Context 스타일
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/malgun.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesKorean());
	

	//	GLFW_IMGUI 연동
	ImGui_ImplGlfw_InitForVulkan(window, true);


	// 디스크립터 풀 준비
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
		throw std::runtime_error("디스크립터 풀 생성 실패.");

	//	ImGui Vulkan 초기화
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
	ImGui_ImplVulkan_NewFrame();
}

//	전체 자원 해제
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
		// 스왑체인 리빌드 할 것
		return;
	}
	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		throw std::runtime_error("Failed to acquire swapchain image.");
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ChatUI::Render(chatMessages, chatCallbacks);

	ImGui::Render();

	VkCommandBuffer cmd = CommandBuffer::commandBuffers[imageIndex];
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// 3. RenderPass 시작
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

	// ImGui DrawData를 실제로 렌더
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);


	vkCmdEndRenderPass(cmd);
	vkEndCommandBuffer(cmd);

	// 큐 제출 (세마포어 동기화)
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

	//	프레젠테이션
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &Swapchain::swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(g_Queue, &presentInfo);

	// VK_ERROR_OUT_OF_DATE_KHR (윈도우 크기 변경 등) 처리
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		// swapchain recreate 필요
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

		// Vulkan 리소스 및 ImGui 초기화
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

		CleanupSocket();
		cleanupAll();
		return 0;
	}
	catch(const std::exception& e)
	{
		std::cerr << "예외 발생 : " << e.what() << std::endl;
		CleanupSocket();
		return -1;
	}
}