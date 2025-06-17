#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace VulkanUtils {
    extern VkPhysicalDevice          physicalDevice;
    extern VkDevice                  device;
    extern VkCommandPool             commandPool;
    extern VkQueue                   graphicsQueue;
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