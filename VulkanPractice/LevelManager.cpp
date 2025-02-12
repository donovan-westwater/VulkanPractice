//#include "common.h"
//#include "ResourceManager.h"
//#include <pxr/usd/usdGeom/xform.h>
//#include <pxr/usd/usd/stage.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include "LevelManager.h"

void LevelManager::testImport() {
	pxr::PlugRegistry::GetInstance().RegisterPlugins(
		"C:/Users/donov/Desktop/Coding Area/Rendering Practice/VulkanPractice/VulkanPractice/Libraries/OpenUSD/plugin/usd/pluginfo.json");
	pxr::PlugPluginPtrVector test = pxr::PlugRegistry::GetInstance().GetAllPlugins();
	for (int i = 0; i < test.size(); i++) {
		std::cout << test[i]->GetName() << "\n";
	}
	std::cout << "-------------------\n";
	testPointer = pxr::UsdStage::Open("dino.obj");//"../../VulkanPratice/Models/dino.obj");
	std::cout << "--Opened dino.obj!--\n";
	testPointer->Export("dinoOut.usd");//"../../VulkanPratice/Models/dino.usd");
	std::cout << "--Exported dino.obj!--\n";
	testPointer->Save();
	std::cout << "--Saved dino.obj!--\n";
}