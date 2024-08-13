#pragma once
#ifndef MODEL_H
#define MODEL_H
#include "common.h"
#include "Mesh.h"

//Make a manager class?
struct Model
{
	glm::mat4x4 modelMatrix;
	Mesh* referenceMesh;
	uint32_t referenceMeshIndex;
	Material* referenceMaterial;
	uint32_t referenceMaterialIndex;
	VkDescriptorSetLayout* referenceLayout;
	VkPipeline* referencePipeline;
	VkDescriptorPool* referencePool;
};
//Make a manager class?

#endif