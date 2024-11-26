#pragma once
#ifndef RAY_TRACING_PIPELINE_H
#define RAY_TRACING_PIPELINE_H

#include "common.h"
#include "ResourceManager.h"

//Forward declaring to avoid circular dependendeices
class RayTracer;

class RayTracingPipeline : public Pipeline {
	//This class should handle the shader resources that come with ray tracing pipelines!
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> raytracingShaderGroups;
	VkBuffer shaderBindingTableBuffer;
	VkDeviceMemory shaderBindingTableDeviceMemory;
	VkStridedDeviceAddressRegionKHR rayGenerationRegion;
	VkStridedDeviceAddressRegionKHR rayMissRegion;
	VkStridedDeviceAddressRegionKHR rayHitRegion;
	VkStridedDeviceAddressRegionKHR rayCallRegion;
public:
	RayTracer* refRayTracer;
	void createRayTracerDescriptorSets();
	void createRayTracerDescriptorPool();
	void createRayTracerDescriptorSetLayout();
	void createShaderBindingTable();
	void createRayTracingPipeline();
	void updateDescriptorSets(uint32_t frameIndex);
	VkStridedDeviceAddressRegionKHR* getShaderRegionAddress(int regionNumber);
	void cleanup();

};
#endif