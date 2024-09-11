#include "Mesh.h"

void Mesh::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    //Create staging buffer for to transfer the index buffer with
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    resourceManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory
        , true);

    void* data;
    vkMapMemory(resourceManager.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(resourceManager.device, stagingBufferMemory);
    VkBufferUsageFlags rayTracingFlags = // used also for building acceleration structures 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    resourceManager.createBuffer(bufferSize, rayTracingFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory
        , true);
#ifndef NDEBUG
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(indexBuffer), "Index Buffer");
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<uint64_t>(indexBufferMemory), "Index Buffer Memory");
#endif
    resourceManager.copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    vkDestroyBuffer(resourceManager.device, stagingBuffer, nullptr);
    vkFreeMemory(resourceManager.device, stagingBufferMemory, nullptr);
}
void Mesh::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    //Staging buffer to transfer data between CPU and GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    resourceManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory,
        true);

    //Create a memory map between the vertex buffer (CPU) and the shader (GPU)
    //This allows both sides to access the data
    void* data;
    vkMapMemory(resourceManager.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(resourceManager.device, stagingBufferMemory);
    VkBufferUsageFlags rayTracingFlags = // used also for building acceleration structures 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    //The vertex buffer itself
    resourceManager.createBuffer(bufferSize, rayTracingFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory
        , true);
#ifndef NDEBUG
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(vertexBuffer), "Vertex Buffer");
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<uint64_t>(vertexBufferMemory), "Vertex Buffer Memory");
#endif
    //Copy data from staging buffer to vertex buffer
    resourceManager.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    //Clean up data after we are done with it
    vkDestroyBuffer(resourceManager.device, stagingBuffer, nullptr);
    vkFreeMemory(resourceManager.device, stagingBufferMemory, nullptr);
}
void Mesh::createMaterialIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(materialIndices[0]) * materialIndices.size();
    //Staging buffer to transfer data between CPU and GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    resourceManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory,
        true);

    //Create a memory map between the vertex buffer (CPU) and the shader (GPU)
    //This allows both sides to access the data
    void* data;
    vkMapMemory(resourceManager.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, materialIndices.data(), (size_t)bufferSize);
    vkUnmapMemory(resourceManager.device, stagingBufferMemory);
    VkBufferUsageFlags rayTracingFlags = // used also for building acceleration structures 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    //The vertex buffer itself
    resourceManager.createBuffer(bufferSize, rayTracingFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, materialIndexBuffer, materialIndexBufferMemory
        , true);
#ifndef NDEBUG
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(materialIndexBuffer), "Material Index Buffer");
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<uint64_t>(materialIndexBufferMemory), "Material Index Buffer Memory");
#endif
    //Copy data from staging buffer to vertex buffer
    resourceManager.copyBuffer(stagingBuffer, materialIndexBuffer, bufferSize);
    //Clean up data after we are done with it
    vkDestroyBuffer(resourceManager.device, stagingBuffer, nullptr);
    vkFreeMemory(resourceManager.device, stagingBufferMemory, nullptr);
}
void Mesh::createMaterialBuffer() {
    VkDeviceSize bufferSize = sizeof(materials[0]) * materials.size();
    //Staging buffer to transfer data between CPU and GPU
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    resourceManager.createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory,
        true);

    //Create a memory map between the vertex buffer (CPU) and the shader (GPU)
    //This allows both sides to access the data
    void* data;
    vkMapMemory(resourceManager.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, materials.data(), (size_t)bufferSize);
    vkUnmapMemory(resourceManager.device, stagingBufferMemory);
    VkBufferUsageFlags rayTracingFlags = // used also for building acceleration structures 
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    //The material buffer itself
    resourceManager.createBuffer(bufferSize, rayTracingFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, materialBuffer, materialBufferMemory
        , true);
#ifndef NDEBUG
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(materialBuffer), "Material Buffer");
    setDebugObjectName(resourceManager.device, VkObjectType::VK_OBJECT_TYPE_DEVICE_MEMORY, reinterpret_cast<uint64_t>(materialBufferMemory), "Material Buffer Memory");
#endif
    //Copy data from staging buffer to vertex buffer
    resourceManager.copyBuffer(stagingBuffer, materialBuffer, bufferSize);
    //Clean up data after we are done with it
    vkDestroyBuffer(resourceManager.device, stagingBuffer, nullptr);
    vkFreeMemory(resourceManager.device, stagingBufferMemory, nullptr);
}

void Mesh::free() {
    vkDestroyBuffer(resourceManager.device, indexBuffer, nullptr);
    vkFreeMemory(resourceManager.device, indexBufferMemory, nullptr);

    vkDestroyBuffer(resourceManager.device, vertexBuffer, nullptr);
    vkFreeMemory(resourceManager.device, vertexBufferMemory, nullptr); //Free the memory assoiated with vertex buffer
}