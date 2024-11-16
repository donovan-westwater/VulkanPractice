#pragma once
#ifndef RAYTRACER_H
#define RAYTRACER_H
#include "common.h"
#include "ResourceManager.h"

//forward declaration
class RayTracingPipeline;

//BLAS Info assioated with the mesh
struct RayTracerMeshInfo {
	VkBuffer bottomLevelAccelerationStructureBuffer; //contiang buffer for each model
	VkDeviceMemory bottomLevelAccelerationStructureDeviceMemory; //containg device memory for each model
	VkAccelerationStructureKHR bottomLevelAccelerationStructure; //contining the struct for each model
	VkDeviceAddress bottomLevelAccelerationStructureAddress; //Where is the memory stored?
	uint32_t referenceMeshIndex; //Index to the mesh in the resource manager this is using
};
//Instance info assioated with models
struct RayTracerModelInstancesInfo {
	VkBuffer modelBottomLevelInstanceBuffer;
	std::vector<VkAccelerationStructureInstanceKHR> modelBottomLevelInstances;
	VkDeviceMemory modelBottomLevelInstanceMemory;
	VkDeviceAddress modelBottomLevelInstanceAddress;
	std::vector<uint32_t> referenceModelIndices; //Indices to the models in the resource manager this is using
};
class RayTracer {
	const int MAX_FRAMES_IN_FLIGHT = 2; //The amount of frames that can be processed concurrently
	RayTracingPipeline *refRayTracingPipeline; //Pointer to a pipeline in the resource manager
	//VkPipeline raytracingPipeline; //Should be placed into the pipeline system?
	//VkPipelineLayout rayPipelineLayout; //SEE ABOVE 
	VkBuffer topLevelAccelerationStructureBuffer;
	VkDeviceMemory topLevelAccelerationStructureDeviceMemory;
	VkAccelerationStructureKHR topLevelAccelerationStructure; //This is the entry point for ray tracing. SHould represent a scene!
	//Trying out having RayTracer manager ray tracer resources and we just sync them up with the resource manager
	std::vector <RayTracerMeshInfo> bottomLevelMeshInfoList; //BLAS Info stored here
	RayTracerModelInstancesInfo bottomLevelModelInstanceInfo; //INstance info for top level acc
	//std::vector <VkBuffer> bottomLevelAccelerationStructureBufferList; //should be Vector contiang buffer for each model
	//std::vector <VkDeviceMemory> bottomLevelAccelerationStructureDeviceMemoryList; //Should becontaing device memory for each model
	//std::vector<VkAccelerationStructureKHR> bottomLevelAccelerationStructureList; //Should be a vector contining the struct for each model
	//Functions
	void createTopLevelAccelerationStructure();
	void recreateTopLevelAccelerationStrucuture();
	void InitalizeMeshInstances();
	void refreshMeshInstances();
	void modelToBottomLevelAccelerationStructure(Mesh& mesh);
	void initRayTracing();

public:
	bool isEnabled = true;
	uint32_t* currentFrameRef;
	uint32_t widthRef;
	uint32_t heightRef;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
	LightSource* mainLightSource;
	VkDevice* mainLogicalDevice; //Logical device chosen by main
	VkPhysicalDevice* mainPhysicalDevice; //physical device chosen by main
	VkSurfaceKHR* mainSurface; //Surface allocated by main
	VkCommandPool* mainCommandPool; //Should point back to the main pool from the main pipeline
	VkQueue* mainGraphicsQueue; //Submission queue for the main pool
	VkFormat* mainSwapChainFormat;
	VkImage rayTracerImage; //Main Image used for ray tracing
	VkDeviceMemory rayTracerImageDeviceMemory; //Allocates memory for rt image
	VkImageView rayTracerImageView; //Image view to access rtImage
	VkDescriptorSetLayout* mainDescSetLayout; //The desc set layout of the rasterization pipeline
	std::vector<VkDescriptorSet>* mainDescSets;
	std::vector<VkFence>* rayTracerFences;
	VkSwapchainKHR* rayTracerSwapchain;
	std::vector<VkImage>* rayTracerSwapchainImages;
	std::vector<VkSemaphore>* rayTracerImageAvailableSemaphores;
	std::vector<VkSemaphore>* rayTracerFinishedSemaphores;
	VkQueue* rayTracerPresentQueue;
	struct PushConstantRay
	{
		glm::vec4 clearColor;
		glm::vec3 lightPos;
		float lightIntensity;
		int lightType;
	};
	PushConstantRay pushConstantRay;
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

	//VkMemoryAllocateFlagsInfo getDefaultAllocationFlags();

	void createRayTracerImageAndImageView();

	void cleanup();
	//Common useful raytrace specfic functions
	uint32_t findSimultaniousGraphicsAndPresentIndex(VkPhysicalDevice phyDevice);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

	uint32_t findBufferMemoryTypeIndex(VkDevice logicalDevice, VkPhysicalDevice physicalDevice
		, VkBuffer buffer, VkMemoryPropertyFlagBits flagBits);

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
	static VkMemoryAllocateFlagsInfo getDefaultAllocationFlags() {
		VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo;
		memoryAllocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
			memoryAllocateFlagsInfo.pNext = NULL,
			memoryAllocateFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
			memoryAllocateFlagsInfo.deviceMask = 0;
		return memoryAllocateFlagsInfo;
	}

	void rayTrace(VkCommandBuffer& cmdBuf, std::vector<void*>& uniBufferMMap, glm::vec4 clearColor);

	VkAccelerationStructureKHR* getTopLevelAccelerationStructure();

	VkAccelerationStructureKHR* getBottomLevelAccelerationStructure(int index);

	void setupRayTracer();

	void CreateLightAndPassVarsToRayTracer();
};

#endif