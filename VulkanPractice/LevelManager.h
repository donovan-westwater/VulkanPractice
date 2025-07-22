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

class LevelManager {
public:
	pxr::UsdStageRefPtr testPointer;
	
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
	void loadPrim(pxr::UsdPrim prim);
};
#endif // !LEVEL_MANAGER_H
