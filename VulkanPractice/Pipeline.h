#pragma once
#ifndef PIPELINE_H
#define PIPELINE_H
#include "common.h"

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
	//Add deleter
};
//Make a manager class?

#endif