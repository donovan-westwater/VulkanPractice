#pragma once
#ifndef MODEL_H
#define MODEL_H
#include "common.h"
#include "Pipeline.h"
#include "Mesh.h"

//Make a manager class?
class Model
{
public:
	glm::mat4x4 modelMatrix;
	Mesh* referenceMesh;
	uint32_t referenceMeshIndex;
	Material* referenceMaterial;
	uint32_t referenceMaterialIndex;
	Pipeline* referencePipeline;


	void loadModel();

};
//Make a manager class?

#endif