#pragma once
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <fstream>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <array>
#include <unordered_map>
#include "Pipeline.h"
#include "Mesh.h"
#include "Model.h"
//Stores the shared pointers used by multiple pipelines
class UniversalResourcePool {
public:
    //Shared Resource Pointers
    //Resources with the most dependcies by declared first
    std::shared_ptr<VkSurfaceKHR> shared_surface;
    std::shared_ptr<VkDevice> shared_logicalDevice;
    std::shared_ptr<VkPhysicalDevice> shared_physicalDevice;
    std::shared_ptr<Pipeline> shared_mainPipeline;
    std::shared_ptr<LightSource> shared_lightSource;
    std::shared_ptr<VkCommandPool> shared_commandPool;
    std::shared_ptr<VkQueue> shared_graphicsQueue;
    std::shared_ptr<VkFormat> shared_swapChainFormat;
    std::shared_ptr<std::vector<VkFence>> shared_fences;
    std::shared_ptr<VkSwapchainKHR> shared_swapchain;
    std::shared_ptr<std::vector<VkImage>> shared_swapchainImages;
    std::shared_ptr<std::vector<VkSemaphore>> shared_imageAvailableSemaphores;
    std::shared_ptr<std::vector<VkSemaphore>> shared_finishedSemaphores;
    std::shared_ptr<VkDescriptorSetLayout> shared_defaultDescSetLayout;
    std::shared_ptr<VkDescriptorSetLayout> shared_defaultDescSetList;
    std::shared_ptr<VkQueue> shared_presentQueue;
    std::shared_ptr<uint32_t> shared_currentFrame;
};
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities; //What basic surface capabilities does the swap chain have?
    std::vector<VkSurfaceFormatKHR>formats; //What surface formats do we have?
    std::vector<VkPresentModeKHR> presentModes; //What available presentation formats?
};
class ResourceManager {
public:
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_ray_tracing_pipeline",
      "VK_KHR_acceleration_structure",
      "VK_EXT_descriptor_indexing",
      "VK_KHR_maintenance3",
      "VK_KHR_buffer_device_address",
      "VK_KHR_deferred_host_operations"
    }; //Provides a list of required extensions for the system
    const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
    "VK_LAYER_LUNARG_monitor"
    }; //Provides a list of required validation layers for the system

    bool useRayTracing = false;
	//Game life resources
    UniversalResourcePool universalResourcePool;
    GLFWwindow* window; //Reference to the window we draw for vulkan
    VkInstance instance; //An instance is the connection between the app and the vulkan lib
    VkDebugUtilsMessengerEXT debugMessenger; //Debug messenger must be made for debug callbacks to be used
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; //Manages API for the physical hardware
    VkDevice device; //Logical device to interface with the physical device
    VkQueue graphicsQueue; //The queue for submitting graphics commands
    VkQueue presentQueue; //The queue for submitting windows surface commands
    VkSurfaceKHR surface; //Handles apis between vulkan and the window system to present results to screen
    VkSwapchainKHR swapChain; //Handles the swapchain
    std::vector<VkImage> swapChainImages; //stores images retrieved from swapchain
    VkFormat swapChainImageFormat; //Format for swap chain images
    VkExtent2D swapChainExtent; //window extents for the images
    std::vector<VkImageView> swapChainImageViews; //Creates an object to use the images from swapchain. its literally a view into an image. Describes how to access the image
    std::vector<VkFramebuffer> swapChainFramebuffers; //references all imageview objects that represent attachments
    VkCommandPool commandPool; //Manages memory used to store buffers used for command buffers
    std::vector<VkCommandBuffer> commandBuffers; //Used to store draw calls
    std::vector<VkSemaphore> imageAvailableSemaphores; //Need to synchronize the GPU calls using semaphores
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector <VkFence> inFlightFences; //Used to for order execution on the cpu to sync with gpu
    std::vector<Pipeline> pipelineList; //Index 0 is the default rasterization pipeline!
    uint32_t currentFrame;
    //Object life resources
	std::vector<Mesh> meshList;
	std::vector<Model> modelList;
    std::vector<LightSource> lightList;
	//Might move materials to be managed here rather than managed by mesh

    void InitUniversalResourcePool();

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

    VkCommandBuffer beginSingleTimeCommands();


    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory, bool rayTracingMemAlloc);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlag, uint32_t mipLevels);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkFormat findDepthFormat();

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    void createSyncObjects();

    void createCommandBuffers();

    void createCommandPool();

    void createImageViews();

    void cleanupSwapChain();

    void recreateSwapChain();

    void createSwapChain();

    void createSurface();

    void createLogicalDevice();

    void pickPhysicalDevice();

    bool isDeviceSuitable(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    VkSurfaceFormatKHR chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    void resourceCleanUp();
};
#endif