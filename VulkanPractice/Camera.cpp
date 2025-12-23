#include "common.h"
#include "ResourceManager.h"
#include "Camera.h"

void Camera::cameraInit() {
    view = glm::lookAt(glm::vec3(0.0f, -20.0f, 10.0f), glm::vec3(0.0f, 20.0f, 10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    proj = glm::perspective(glm::radians(45.0f), ResourceManager::manager->swapChainExtent.width / (float)ResourceManager::manager->swapChainExtent.height, 0.1f, 200.0f);
    proj[1][1] *= -1;
}
static float totalCamT = 0;
void Camera::updateCamera() {
    float delta = ResourceManager::manager->deltaTime;
    totalCamT += delta;
    view = glm::lookAt(glm::vec3(0.0f, 00.0f - 1.0f * totalCamT, 10.0f), glm::vec3(0.0f, 40.0f- 1.0f * totalCamT, 10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    //view = glm::rotate(view, delta*glm::radians(10.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    //Create a perspective based projection matrix for our camera
    //proj = glm::perspective(glm::radians(45.0f), ResourceManager::manager->swapChainExtent.width / (float)ResourceManager::manager->swapChainExtent.height, 0.1f, 20.0f);
    //proj[1][1] *= -1; //Y-coord for clip coords is inverted. This fixes that (GLM designed for openGL)
}