#pragma once
#ifndef MESH_H
#define MESH_H
#include "common.h"
#include "Texture.h"
//Move into its own header and make a class for manager
class Mesh {
public:
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<Material> materials;
	std::vector<uint32_t> materialIndices;
	uint32_t vertexCount;
	uint32_t indexCount;
	VkBuffer vertexBuffer; //The vertex buffer we pass during the vertex shader step
	VkDeviceMemory vertexBufferMemory; //handle to deal with allocated memory to vertex buffer
	VkBuffer indexBuffer; //Index buffer to prevent bloat in vertex buffer
	VkDeviceMemory indexBufferMemory; //handle to deal with memory allocated with the index buffer
	VkBuffer materialBuffer; //Material buffer we can pass to shaders
	VkDeviceMemory materialBufferMemory; //handle to deal with memory allocated with the material buffer
	VkBuffer materialIndexBuffer; //Index buffer to prevent bloat in material buffer
	VkDeviceMemory materialIndexBufferMemory; //handle to deal with memory allocated with the material index buffer
	//Functions section
	void createIndexBuffer();
	void createVertexBuffer();
	void createMaterialIndexBuffer();
	void createMaterialBuffer();
	//Add texture infomation here later
	Texture texture;
	//Add Delete function
};



#endif