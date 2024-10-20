#include "common.h"
#include "ResourceManager.h"
#include "Camera.h"

void Camera::cameraInit() {
    view = glm::lookAt(glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    proj = glm::perspective(glm::radians(45.0f), ResourceManager::manager->swapChainExtent.width / (float)ResourceManager::manager->swapChainExtent.height, 0.1f, 10.0f);
    proj[1][1] *= -1;
}

void Camera::updateCamera() {
    
    view = glm::lookAt(glm::vec3(5.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    //Create a perspective based projection matrix for our camera
    proj = glm::perspective(glm::radians(45.0f), ResourceManager::manager->swapChainExtent.width / (float)ResourceManager::manager->swapChainExtent.height, 0.1f, 10.0f);
    proj[1][1] *= -1; //Y-coord for clip coords is inverted. This fixes that (GLM designed for openGL)
}