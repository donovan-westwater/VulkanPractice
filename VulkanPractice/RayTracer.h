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
	PushConstantRay pcRay;
	VkBuffer bASSBuffer;
	VkBuffer tASSBuffer;
	VkAccelerationStructureKHR blAShandle;
	VkAccelerationStructureKHR tlAShandle;
	VkDescriptorSetLayout descriptorSetLayout; //holds values to setup the descriptor sets
	std::vector<VkDescriptorSet> descriptorSets; //The actual descr sets
	VkDescriptorPool descriptorPool;
	VkBuffer sbtBuffer;
	VkStridedDeviceAddressRegionKHR rayGenRegion;
	VkStridedDeviceAddressRegionKHR rayMissRegion;
	VkStridedDeviceAddressRegionKHR rayHitRegion;
	VkStridedDeviceAddressRegionKHR rayCallRegion;
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties;
	//Functions
	VkShaderModule createShaderModule(const std::vector<char>& code);
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
	void createTopLevelAS();
	void createRtDescriptorSets();
	void createRtDescriptorPool();
	void createRtDescriptorSetLayout();
	void createShaderBindingTable();
	void modelToBLAS(VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t nOfVerts);
	void initRayTracing();
	VkMemoryAllocateFlagsInfo getDefaultAllocationFlags();
public:
	bool isEnabled = true;
	uint32_t* currentFrameRef;
	uint32_t widthRef;
	uint32_t heightRef;
	LightSource* rastSource;
	VkDevice* mainLogicalDevice; //Logical device chosen by main
	VkPhysicalDevice* mainPhysicalDevice; //physical device chosen by main
	VkSurfaceKHR* mainSurface; //Surface allocated by main
	VkCommandPool* mainCommandPool; //Should point back to the main pool from the main pipeline
	VkQueue* mainGraphicsQueue; //Submission queue for the main pool
	VkImageView* rtColorBufferView; //Should point toward color buffer in main
	VkDescriptorSetLayout* mainDescSetLayout; //The desc set layout of the rasterization pipeline
	std::vector<VkDescriptorSet>* mainDescSets;
	//Code taken from https://github.com/WilliamLewww/vulkan_ray_tracing_minimal_abstraction/blob/master/ray_pipeline/src/main.cpp
	//Around the lines around #380
	PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR;

	PFN_vkCreateRayTracingPipelinesKHR pvkCreateRayTracingPipelinesKHR;

	PFN_vkGetAccelerationStructureBuildSizesKHR pvkGetAccelerationStructureBuildSizesKHR;

	PFN_vkCreateAccelerationStructureKHR pvkCreateAccelerationStructureKHR;

	PFN_vkDestroyAccelerationStructureKHR pvkDestroyAccelerationStructureKHR;

	PFN_vkGetAccelerationStructureDeviceAddressKHR pvkGetAccelerationStructureDeviceAddressKHR;

	PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR;

	PFN_vkGetRayTracingShaderGroupHandlesKHR pvkGetRayTracingCaptureReplayShaderGroupHandlesKHR;

	PFN_vkCmdTraceRaysKHR pvkCmdTraceRaysKHR;

	void Cleanup();

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

	void raytrace(VkCommandBuffer& cmdBuf, glm::vec4 clearColor);

	void createRayTracingPipeline();

	void updateRTDescriptorSets();

	void setupRayTracer(VkBuffer& vertexBuffer, VkBuffer& indexBuffer, uint32_t nOfVerts);

};

#endif