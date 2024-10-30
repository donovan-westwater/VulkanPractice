#pragma once
#ifndef MODEL_H
#define MODEL_H
#include "common.h"
#include "ResourceManager.h"
#include "Pipeline.h"
#include "Mesh.h"
//Make a manager class?
class Model
{
public:
	glm::mat4x4 modelMatrix;
	uint32_t referenceMeshIndex;
	uint32_t referencePipelineIndex;
	uint32_t referenceTextureIndex;
	uint32_t allocatedDescSetIndex;
	uint32_t resourceListIndex; //Where is this mesh inside the resourece manager?
	uint32_t referenceRayTracerModelInfoIndex;
	//UBO section
	std::vector<VkBuffer> uniformBuffers; //ubo buffer
	std::vector<VkDeviceMemory> uniformBuffersMemory;//handle to allocated buffer memory
	//Add RayTracing Instance info for top level acceleration struct here

	void loadModel(std::string modelPath, std::string materialPath, std::string texturePath);
	void createUniformBuffers();
	void updateUniformBuffers(uint32_t frameNum);
	void testUpdate();
	void setScale(glm::vec3 scale);
	void setPosition(glm::vec3 pos);
	void setRotation(glm::vec3 eulerAngles);
};
//Make a manager class?

#endif