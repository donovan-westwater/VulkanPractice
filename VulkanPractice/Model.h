#pragma once
#ifndef MODEL_H
#define MODEL_H
#include "common.h"
#include "ResourceManager.h"
#include "Pipeline.h"
#include "Mesh.h"
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
//Make a manager class?
class Model
{
public:
	glm::mat4x4 modelMatrix;
	uint32_t referenceMeshIndex; //Where is this mesh inside the resourece manager?
	uint32_t referencePipelineIndex;
	uint32_t referenceTextureIndex;
	uint32_t allocatedDescSetIndex;
	uint32_t resourceListIndex; 
	uint32_t referenceRayTracerModelInfoIndex;
	//UBO section
	std::vector<VkBuffer> uniformBuffers; //ubo buffer
	std::vector<VkDeviceMemory> uniformBuffersMemory;//handle to allocated buffer memory
	//Add RayTracing Instance info for top level acceleration struct here

	void loadModel(std::string modelPath, std::string materialPath, std::string texturePath);
	void loadModel(pxr::UsdPrim prim,pxr::UsdPrim matPrim);
	void createUniformBuffers();
	void updateUniformBuffers(uint32_t frameNum);
	void testUpdate();
	void setScale(glm::vec3 scale);
	void setPosition(glm::vec3 pos);
	void setRotation(glm::vec3 eulerAngles);
	void rotateInPlace(glm::vec3 deltaAngles);
private:
	template<typename T>
	T loadMatValue(pxr::UsdShadeShader shader, std::string propName) {
		T outValue;
		pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(propName));
		input.Get(&outValue);
		return outValue;
	}
};
//Make a manager class?

#endif