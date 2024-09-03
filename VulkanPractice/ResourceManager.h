#pragma once
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H
#include "common.h"
#include "Pipeline.h"
#include "Mesh.h"
#include "Model.h"
class ResourceManager {
public:
	static ResourceManager manager;
	//Game life resources
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
    std::vector<VkPipeline> pipelineList; //Index 0 is the default rasterization pipeline!
    
    //Object life resources
	std::vector<Mesh> meshList;
	std::vector<Model> modelList;
	//Might move materials to be managed here rather than managed by mesh
	

};
#endif