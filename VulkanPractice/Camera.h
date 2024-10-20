#pragma once
#ifndef CAMERA_H
#define CAMERA_H
#include "common.h"
#include "ResourceManager.h"
class Camera {
public:
	glm::mat4x4 view;
	glm::mat4x4 proj;
	//Should be able to init
	void cameraInit();
	//Should be able to transfer info to UBO
	void updateCamera();
	//Should be able to apply transforms onto it	
	//---Rotations---
	//---Translations---
	//Add Delete function
};

#endif