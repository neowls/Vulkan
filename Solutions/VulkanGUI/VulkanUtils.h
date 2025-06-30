#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace VulkanUtils 
{
    // Vulkan 리소스 핸들
    extern VkAllocationCallbacks*    g_Allocator;
    extern VkInstance                g_Instance;
    extern uint32_t                  g_QueueFamily;
    extern VkPipelineCache           g_PipelineCache;
    extern VkDescriptorPool          g_DescriptorPool;
    extern uint32_t                  g_MinImageCount;
    extern bool                      g_SwapChainRebuild;
    extern VkPhysicalDevice          g_PhysicalDevice;
    extern VkDevice                  g_Device;
    extern VkCommandPool             g_CommandPool;
    extern VkQueue                   g_Queue;
    extern VkDebugUtilsMessengerEXT  debugMessenger;

    VkFormat   findSupportedFormat(const std::vector<VkFormat>&, VkImageTiling, VkFormatFeatureFlags);
    VkFormat   findDepthFormat();
    bool       hasStencilComponent(VkFormat);

    uint32_t   findMemoryType(uint32_t, VkMemoryPropertyFlags);

    void       createImage(uint32_t, uint32_t, VkFormat, VkImageTiling, VkImageUsageFlags,
        VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&);
    VkCommandBuffer beginSingleTimeCommands();
    void       endSingleTimeCommands(VkCommandBuffer);
    void       transitionImageLayout(VkImage, VkFormat, VkImageLayout, VkImageLayout);

    std::vector<char> readFile(const std::string& path);
    VkShaderModule    createShaderModule(const std::vector<char>&);
    
    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT*, void*);
    void       populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT&);
    VkResult   CreateDebugUtilsMessengerEXT(VkInstance, const VkDebugUtilsMessengerCreateInfoEXT*,
        const VkAllocationCallbacks*, VkDebugUtilsMessengerEXT*);
    void       DestroyDebugUtilsMessengerEXT(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);
}