#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

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

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
//Vertex attributes: assigned per vertex
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;
    //Our vertex Binding description: how to bind vertex to the shader in the GPU
    static VkVertexInputBindingDescription getBindingDescription() {
        //All of our per vertex data is packed in one array. Only need 1 binding
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0; //Index to bind the array to
        bindingDescription.stride = sizeof(Vertex); //How far are the data points in the array?
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //Movee to the next data entry after each vertex
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{}; //one for vertex pos, one for vetex color
        attributeDescriptions[0].binding = 0; //Which binding our we getting our vertex info from?
        attributeDescriptions[0].location = 0; //Which location in the vertex shader should this go to?
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos); //num of bytes from the start of the per vetex data to read from
        //The above is for positions, so we offset to the start of the pos var in the struct

        attributeDescriptions[1].binding = 0; //Which binding our we getting our vertex info from?
        attributeDescriptions[1].location = 1; //Which location in the vertex shader should this go to?
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT; //How much data are we providing to each channel?
        attributeDescriptions[1].offset = offsetof(Vertex, color); //num of bytes from the start of the per vetex data to read from

        attributeDescriptions[2].binding = 0; //Which binding our we getting our vertex info from?
        attributeDescriptions[2].location = 2; //Which location in the vertex shader should this go to?
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT; //How much data are we providing to each channel?
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord); //num of bytes from the start of the per vetex data to read from


        return attributeDescriptions;
    }
    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }

};
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}