#pragma once
#ifndef LEVEL_MANAGER_H
#define LEVEL_MANAGER_H

#include "common.h"
#include "ResourceManager.h"
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

class LevelManager {
public:
	inline static LevelManager* currentLevel = nullptr;
	pxr::UsdStageRefPtr testPointer;
	std::vector<Vertex> sceneVertices;
	std::vector<uint32_t> sceneIndices;
	std::vector<Material> sceneMaterials;
	std::vector<uint32_t> sceneMaterialIndices;
	std::vector<uint32_t> sceneOffsets;
	VkBuffer vertexBuffer; //The vertex buffer we pass during the vertex shader step
	VkDeviceMemory vertexBufferMemory; //handle to deal with allocated memory to vertex buffer
	VkBuffer indexBuffer; //Index buffer to prevent bloat in vertex buffer
	VkDeviceMemory indexBufferMemory; //handle to deal with memory allocated with the index buffer
	VkBuffer materialBuffer; //Material buffer we can pass to shaders
	VkDeviceMemory materialBufferMemory; //handle to deal with memory allocated with the material buffer
	VkBuffer materialIndexBuffer; //Index buffer to prevent bloat in material buffer
	VkDeviceMemory materialIndexBufferMemory; //handle to deal with memory allocated with the material index buffer
	VkBuffer offsetBuffer;
	VkDeviceMemory offsetBufferMemory;
	static void loadPlugins() {
		//The Local Library Dlls seem to be breaking the plugins?
#ifdef NDEBUG
		pxr::PlugRegistry::GetInstance().RegisterPlugins(
		"C:/Users/donov/Desktop/Coding Area/Rendering Practice/VulkanPractice/VulkanPractice/Libraries/OpenUSD/plugin/usd/pluginfo.json");
#else
		pxr::PlugRegistry::GetInstance().RegisterPlugins(
			"C:/Users/donov/Desktop/Coding Area/Rendering Practice/VulkanPractice/VulkanPractice/Libraries/OpenUSD-Debug/OpenUSD/plugin/usd/pluginfo.json");
#endif
		pxr::PlugPluginPtrVector test = pxr::PlugRegistry::GetInstance().GetAllPlugins();
		for (int i = 0; i < test.size(); i++) {
			std::cout << test[i]->GetName() << "\n";
		}
	}

	void testImportAndExport();
	void loadLevel(std::string levelName);
private:
	template<typename T>
	T loadMatValue(pxr::UsdShadeShader shader, std::string propName) {
		T outValue;
		pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(propName));
		input.Get(&outValue);
		return outValue;
	}
	void loadPrim(pxr::UsdPrim prim);
	void createSceneBuffers();
	void recreateSceneBuffers();
};
#endif // !LEVEL_MANAGER_H
