#pragma once
#ifndef COMMON_H
#define COMMON_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>


#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>
#include <algorithm>
#include <fstream>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <array>
#include <unordered_map>


struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    //glm::vec4 colorAdd;
};
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

inline bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
struct LightSource {
    glm::vec3 pos;
    glm::vec3 dir;
    float intensity;
    int type;
};
//Vertex attributes: assigned per vertex
struct Vertex {
    alignas(16)glm::vec3 pos;
    alignas(16)glm::vec3 normal;
    alignas(16)glm::vec3 color;
    alignas(8) glm::vec2 texCoord;
    //Our vertex Binding description: how to bind vertex to the shader in the GPU
    static VkVertexInputBindingDescription getBindingDescription() {
        //All of our per vertex data is packed in one array. Only need 1 binding
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0; //Index to bind the array to
        bindingDescription.stride = sizeof(Vertex); //How far are the data points in the array?
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //Movee to the next data entry after each vertex
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{}; //one for vertex pos, one for vetex color
        attributeDescriptions[0].binding = 0; //Which binding our we getting our vertex info from?
        attributeDescriptions[0].location = 0; //Which location in the vertex shader should this go to?
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos); //num of bytes from the start of the per vetex data to read from
        //The above is for positions, so we offset to the start of the pos var in the struct
        attributeDescriptions[1].binding = 0; //Which binding our we getting our vertex info from?
        attributeDescriptions[1].location = 1; //Which location in the vertex shader should this go to?
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; //How much data are we providing to each channel?
        attributeDescriptions[1].offset = offsetof(Vertex, normal); //num of bytes from the start of the per vetex data to read from

        attributeDescriptions[2].binding = 0; //Which binding our we getting our vertex info from?
        attributeDescriptions[2].location = 2; //Which location in the vertex shader should this go to?
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT; //How much data are we providing to each channel?
        attributeDescriptions[2].offset = offsetof(Vertex, color); //num of bytes from the start of the per vetex data to read from

        attributeDescriptions[3].binding = 0; //Which binding our we getting our vertex info from?
        attributeDescriptions[3].location = 3; //Which location in the vertex shader should this go to?
        attributeDescriptions[3].format = VK_FORMAT_R32G32_SFLOAT; //How much data are we providing to each channel?
        attributeDescriptions[3].offset = offsetof(Vertex, texCoord); //num of bytes from the start of the per vetex data to read from

        
        return attributeDescriptions;
    }
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord
            && normal == other.normal;
    }

};
struct Material {
    glm::vec4 ambient = { 0, 0, 0, 0 };
    glm::vec4 diffuse = { 0, 0, 0, 0 };
    glm::vec4 specular = { 0, 0, 0, 0 };
    glm::vec4 emission = { 0, 0, 0, 0 };
};
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
};
//Deleters for smart pointers
//DEBUG TIP: Remember to look at the call stack when figure out which resources wasnt deleted properly. Very helpfuL!
struct VulkanSmartDeleter {
    VkDevice* logicalDevice;
    VkInstance* instance;

    //Make different overloaded operators for each resource
    void operator()(VkDescriptorSetLayout* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Deleting DescriptorSetLayout!\n";
        vkDestroyDescriptorSetLayout(*logicalDevice, *p, nullptr);
    }
    void operator()(std::vector<VkDescriptorSet>* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Cleaning up Descriptor Set Vector\n";
        for(VkDescriptorSet ds : *p) {
            ds = VK_NULL_HANDLE;
        }
    }
    void operator()(VkDescriptorPool* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Deleting DescriptorPool!\n";
        vkDestroyDescriptorPool(*logicalDevice, *p, nullptr);
    }
    void operator()(VkCommandPool* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Deleting command pool!\n";
        vkDestroyCommandPool(*logicalDevice, *p, nullptr);
    }
    void operator()(VkSwapchainKHR* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Deleting swapchain!\n";
        vkDestroySwapchainKHR(*logicalDevice, *p, nullptr);
        
    }
    void operator()(VkPhysicalDevice* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Deleting Physical Device\n";
        *p = VK_NULL_HANDLE;
    }
    void operator()(VkDevice* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Deleting Logical Device\n";
        vkDestroyDevice(*p, nullptr);
    }
    void operator()(VkSurfaceKHR* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Deleting Surface!\n";
        vkDestroySurfaceKHR(*instance, *p, nullptr);
        std::cout << "Deleting Instance!\n";
        vkDestroyInstance(*instance, nullptr);
    }
    void operator()(LightSource* p) noexcept {
        if (p == nullptr) return;
        std::cout << "zeroing out Light Source\n";
        p->intensity = 0;
        p->type = 0;
        p->pos = glm::vec3(0, 0, 0);
        p->dir = glm::vec3(0, 0, 0);
    }
    void operator()(uint32_t* p) noexcept {
        if (p == nullptr) return;
        std::cout << "zeroing out uint\n";
        *p = 0;
    }
    void operator()(VkQueue* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Queue nulled out\n";
        *p = VK_NULL_HANDLE;
    }
    void operator()(std::vector<VkSemaphore>* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Semaphore deleted\n";
        for (size_t i = 0; i < p->size(); i++) {
            vkDestroySemaphore(*logicalDevice, (*p)[i], nullptr);
        }
    }
    void operator()(std::vector<VkFence>* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Fence deleted\n";
        for (size_t i = 0; i < p->size(); i++) {
            vkDestroyFence(*logicalDevice, (*p)[i], nullptr);
        }
    }
    void operator()(std::vector<VkImage>* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Image deleted\n";
        for (size_t i = 0; i < p->size(); i++) {
            vkDestroyImage(*logicalDevice, (*p)[i], nullptr);
        }
    }
    void operator()(VkFormat* p) noexcept {
        if (p == nullptr) return;
        std::cout << "Format deleted\n";
        *p = VK_FORMAT_UNDEFINED;
    }
};
static glm::vec4 float3ToVec4(float a[3]) {
    glm::vec4 out;
    out.x = a[0];
    out.y = a[1];
    out.z = a[2];
    return out;
}
//Debug Code
#ifndef NDEBUG

static PFN_vkSetDebugUtilsObjectNameEXT pvkSetDebugUtilsObjectNameEXT;

inline static void setDebugObjectName(VkDevice device,VkObjectType objType,uint64_t objHandle,std::string name) {
    VkDebugUtilsObjectNameInfoEXT objName;
    objName.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objName.pNext = NULL;
    objName.pObjectName = name.c_str();
    objName.objectType = objType;
    objName.objectHandle = objHandle;
    VkResult result = pvkSetDebugUtilsObjectNameEXT(device, &objName);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to set debug name for object");
    }
}
#endif 

#endif