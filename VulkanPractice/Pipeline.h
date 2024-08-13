#pragma once
#ifndef PIPELINE_H
#define PIPELINE_H
#include "common.h"

//Should contain the resources we use for the specific pipeline
//The ray tracing pipeline wont be put here since the ray tracer should handle its own pipeline!
struct Pipeline {
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;

	//Add deleter
};
//Make a manager class?

#endif