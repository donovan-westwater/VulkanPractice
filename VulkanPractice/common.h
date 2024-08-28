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
inline static glm::vec4 float3ToVec4(float a[3]) {
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
//All of the common vulkan functions should go here
//Transition layer, create buffer, create image etc.
//Any function left over in main should go here as a static function
//Dont make them inline! Debug is fine since it is small

//Create, allocate, and bind the image to memory
static void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage
    , VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    //Settings for the image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    //We need same format for the texels as the pixels in the buffer
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; //discard textuals first transition
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = numSamples; //Used for multisampling
    imageInfo.flags = 0; // Optional

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }
    //allocating memory to the image
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }
    //Bind the image to the allocated memory
    vkBindImageMemory(device, image, imageMemory, 0);
}
//begins a command buffer to be used for a single use
static VkCommandBuffer beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    //Memory transfer is executed using command buffers
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    //Start recording command buffer to transfer memory with
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

//ends the recording of a single frame
static void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    //Submit the command buffer to the queue
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue); //Wait for the transfer queue to become idle
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer); //Cleanup once we are done with the buffer
}

//Checking the physical memory of our GPU to see if we have room
static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    //Check the physical memory of our device
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    //Find a memory type that will work for our buffer
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) { //Use a bitmask to specify the bit field of suitible memory types
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory
    , bool rayTracingMemAlloc) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size; //size of our buffer
    bufferInfo.usage = usage; //what kind of buffer is this?
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }
    //Query the memory requirements to make sure we have enough space to allocate for the vertex buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
    //Allocate memory for the buffer
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
    VkMemoryAllocateFlagsInfo rayTraceMemInfo;
    if (rayTracingMemAlloc) {
        rayTraceMemInfo = rayTracer.getDefaultAllocationFlags();
        allocInfo.pNext = &rayTraceMemInfo;
    }
    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }
    //Bind the allocated memory to the vertex buffer
    vkBindBufferMemory(device, buffer, bufferMemory, 0);
#ifndef NDEBUG
    setDebugObjectName(device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(buffer), "Temp Buffer");
    setDebugObjectName(device, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<uint64_t>(bufferMemory), "Temp Buffer Memory");
#endif
}

//Copying vulkan buffer from src to dst 
static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    //Declare a region of the memory to copy
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional Where are we copying from?
    copyRegion.dstOffset = 0; // Optional Where are we copying to?
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    endSingleTimeCommands(commandBuffer);

}

//Creates an image view
static VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlag, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    //subresource Range descibes the images's purpose and what part should be accesssed
    //THese imges are color targets without any mpimpaping levels or multiple layers
    viewInfo.subresourceRange.aspectMask = aspectFlag;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

static void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    //pipeline barrir is used to synchonoize resources
    //Ensures buffer completes before reading from it
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image; //specify image effected by barrier
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0; // TODO Need to specify which operations need to do
    barrier.dstAccessMask = 0; // TODO
    VkPipelineStageFlags sourceStage; //Makes sure that we don't need to wait on anything
    VkPipelineStageFlags destinationStage; //Makes sure the shader waits on transfer writes
    //check for stenicl
    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    else {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    //Undefined --> transfer destination
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } //Transfer dest --> Shader Reading
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }//Check for stenicl
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

//Copy buffer to image
static void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    //Selects a region to copy the image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };
    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );
    endSingleTimeCommands(commandBuffer);
}

static bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

//Find a format that works with depth testing
static VkFormat findDepthFormat() {
    return findSupportedFormat(
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

static VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}
#endif