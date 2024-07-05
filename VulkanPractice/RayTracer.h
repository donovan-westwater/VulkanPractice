#pragma once
#ifndef RAYTRACER_H
#define RAYTRACER_H
#include "common.h"
class RayTracer {
	const int MAX_FRAMES_IN_FLIGHT = 2; //The amount of frames that can be processed concurrently
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> raytracingShaderGroups;
	VkPipeline raytracingPipeline;
	VkPipelineLayout rayPipelineLayout;
	struct PushConstantRay
	{
		glm::vec4 clearColor;
		glm::vec3 lightPos;
		float lightIntensity;
		int lightType;
	};
	PushConstantRay pushConstantRay;
	VkBuffer bottomLevelAccelerationStructureBuffer;
	VkBuffer topLevelAccelerationStructureBuffer;
	VkDeviceMemory topLevelAccelerationStructureDeviceMemory;
	VkDeviceMemory bottomLevelAccelerationStructureDeviceMemory;
	VkAccelerationStructureKHR topLevelAccelerationStructure;
	VkAccelerationStructureKHR bottomLevelAccelerationStructure;
	VkDescriptorSetLayout descriptorSetLayout; //holds values to setup the descriptor sets
	std::vector<VkDescriptorSet> descriptorSets; //The actual descr sets
	VkDescriptorPool descriptorPool;
	VkBuffer shaderBindingTableBuffer;
	VkDeviceMemory shaderBindingTableDeviceMemory;
	VkStridedDeviceAddressRegionKHR rayGenerationRegion;
	VkStridedDeviceAddressRegionKHR rayMissRegion;
	VkStridedDeviceAddressRegionKHR rayHitRegion;
	VkStridedDeviceAddressRegionKHR rayCallRegion;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
	//Functions
	VkShaderModule createShaderModule(const std::vector<char>& code);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	uint32_t findSimultaniousGraphicsAndPresentIndex(VkPhysicalDevice phyDevice);
	uint32_t findBufferMemoryTypeIndex(VkDevice logicalDevice, VkPhysicalDevice physicalDevice
		, VkBuffer buffer, VkMemoryPropertyFlagBits flagBits);
	void createTopLevelAccelerationStructure();
	void createRayTracerDescriptorSets(VkBuffer& vertexBuffer, VkBuffer& indexBuffer);
	void createRayTracerDescriptorPool();
	void createRayTracerDescriptorSetLayout();
	void createShaderBindingTable();
	void modelToBottomLevelAccelerationStructure(VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t nOfVerts);
	void initRayTracing();

public:
	bool isEnabled = true;
	int maxPrimativeCount = 0;
	std::weak_ptr<uint32_t> currentFrameRef;
	uint32_t widthRef;
	uint32_t heightRef;
	std::weak_ptr<LightSource> mainLightSource;
	std::weak_ptr<VkDevice> mainLogicalDevice; //Logical device chosen by main
	std::weak_ptr<VkPhysicalDevice> mainPhysicalDevice; //physical device chosen by main
	std::weak_ptr<VkSurfaceKHR> mainSurface; //Surface allocated by main
	std::weak_ptr<VkCommandPool> mainCommandPool; //Should point back to the main pool from the main pipeline
	std::weak_ptr<VkQueue> mainGraphicsQueue; //Submission queue for the main pool
	std::weak_ptr<VkFormat> mainSwapChainFormat;
	VkImage rayTracerImage; //Main Image used for ray tracing
	VkDeviceMemory rayTracerImageDeviceMemory; //Allocates memory for rt image
	VkImageView rayTracerImageView; //Image view to access rtImage
	std::weak_ptr<VkDescriptorSetLayout> mainDescSetLayout; //The desc set layout of the rasterization pipeline
	std::weak_ptr<std::vector<VkDescriptorSet>> mainDescSets;
	std::weak_ptr<std::vector<VkFence>> rayTracerFences;
	std::weak_ptr<VkSwapchainKHR> rayTracerSwapchain;
	std::weak_ptr<std::vector<VkImage>> rayTracerSwapchainImages;
	std::weak_ptr<std::vector<VkSemaphore>> rayTracerImageAvailableSemaphores;
	std::weak_ptr<std::vector<VkSemaphore>> rayTracerFinishedSemaphores;
	std::weak_ptr<VkQueue> rayTracerPresentQueue;
	//Code taken from https://github.com/WilliamLewww/vulkan_ray_tracing_minimal_abstraction/blob/master/ray_pipeline/src/main.cpp
	//Around the lines around #380
	PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR;

	PFN_vkCreateRayTracingPipelinesKHR pvkCreateRayTracingPipelinesKHR;

	PFN_vkGetAccelerationStructureBuildSizesKHR pvkGetAccelerationStructureBuildSizesKHR;

	PFN_vkCreateAccelerationStructureKHR pvkCreateAccelerationStructureKHR;

	PFN_vkDestroyAccelerationStructureKHR pvkDestroyAccelerationStructureKHR;

	PFN_vkGetAccelerationStructureDeviceAddressKHR pvkGetAccelerationStructureDeviceAddressKHR;

	PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR;

	PFN_vkGetRayTracingShaderGroupHandlesKHR pvkGetRayTracingShaderGroupHandlesKHR;

	PFN_vkCmdTraceRaysKHR pvkCmdTraceRaysKHR;

	VkMemoryAllocateFlagsInfo getDefaultAllocationFlags();

	void createRayTracerImageAndImageView();

	void cleanup();

	static std::vector<char> readFile(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}
		//start at the beginning to see the file size and allocate buffer
		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	void rayTrace(VkCommandBuffer& cmdBuf, std::vector<void*>& uniBufferMMap, glm::vec4 clearColor);

	void createRayTracingPipeline();

	void updateRayTracerDescriptorSets();

	void setupRayTracer(VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t nOfVerts);

};

#endif