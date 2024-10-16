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

	void loadModel(std::string modelPath, std::string materialPath, std::string texturePath);

};
//Make a manager class?

#endif