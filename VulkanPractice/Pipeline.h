#pragma once
#ifndef PIPELINE_H
#define PIPELINE_H
#include "common.h"
#include "ResourceManager.h"
#include "Model.h"
//Should contain the resources we use for the specific pipeline
//The ray tracing pipeline wont be put here since the ray tracer should handle its own pipeline!
class Pipeline {
public:
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	std::vector<VkDescriptorSet> descriptorSets;
	VkDescriptorPool descriptorPool;
	VkRenderPass renderPass; //The render pass used to render images
	//Depth and Image resources in case the pipeline wants them
	VkImage depthImage; //Image for depth buffer
	VkDeviceMemory depthImageMemory; //Memory allocated for depth buffer
	VkImageView depthImageView; //View for depth test
	VkImage colorImage; //Color buffer image used to for multi sampling
	VkDeviceMemory colorImageMemory; //Handle for memory allocated image used for multi sampling
	VkImageView colorImageView; //var used to access color buffer used for multisampling
	//UBO section
	std::vector<VkBuffer> uniformBuffers; //ubo buffer
	std::vector<VkDeviceMemory> uniformBuffersMemory;//handle to allocated buffer memory
	std::vector<void*> uniformBuffersMapped; //Buffer for staging
	//Default pipeline settings
	//Anti-Aliasing Resources. Leave in main layer for now
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;//How many times are we sampling for rasterization? reduces jagged edges
	bool isDefaultPipeline = false;
	//Besides the MainGraphics pipeline function, the rest should be private
	void createMainDescriptorSets();
	void createMainDescriptorPool();
	void createMainDescriptorSetLayout();
	void createMainRenderPass();
	void createColorResources();
	void createDepthResources();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void createDefaultGraphicsPipeline();
	void createFramebuffers();
	void createMainUniformBuffers();
	void updateMainUniformBuffers(uint32_t currentFrame);
	void recordDrawCallCommandBuffer(VkCommandBuffer commandBuffer,Model m, uint32_t imageIndex);
	//Add deleter
	void free();
};
//Make a manager class?

#endif