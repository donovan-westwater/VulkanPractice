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
	//Add deleter
};
//Make a manager class?

#endif