#pragma once
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include "common.h"
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
class ResourceManager {
public:
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
};
#endif