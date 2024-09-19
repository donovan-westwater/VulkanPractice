#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "common.h"
#include "ResourceManager.h"
class Texture {
public :
    //Texture Resources
    std::string path;
    uint32_t mipLevels; //mipsampling levels. Used for LOD 
    VkImage textureImage; //image to hold the texture
    VkImageView textureImageView; //Images are accessed indirectly through image views, so the texture will need one
    VkDeviceMemory textureImageMemory; //memory allocated for the texture
    VkSampler textureSampler;//Texture sampler for shader
    //Management functions
    //Create teture image to load textures with
    void createTextureImage(std::string texturePath,VkDevice &device);
    void createImageTextureView(VkDevice& device);
    void createTextureSampler(VkDevice& device, VkPhysicalDevice& physicalDevice);
    void loadTexture(std::string texturePath, VkDevice& device, VkPhysicalDevice& physicalDevice);
    void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
    //Delete function here
    void free();
};
#endif