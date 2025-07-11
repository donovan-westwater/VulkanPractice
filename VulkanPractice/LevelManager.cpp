//#include "common.h"
//#include "ResourceManager.h"
//#include <pxr/usd/usdGeom/xform.h>
//#include <pxr/usd/usd/stage.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformOp.h>
#include <pxr/usd/usd/stage.h>
#include "LevelManager.h"
#include <filesystem>

void LevelManager::testImport() {
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
	//usd is crashing when opening dino.obj --> I can open dino.obj when I run it via python
	//Something is wrong with the C++ version specifically?
	std::cout << "-------------------\n";
	bool supported = pxr::UsdStage::IsSupportedFile("dino.obj");
	if (!supported) {
		std::cout << "NOT SUPPORTED!\n";
	}
	std::cout << std::filesystem::current_path()<<"\n";
	if(std::filesystem::exists("Models/dino.obj")) std::cout << "Found";
	testPointer = pxr::UsdStage::Open("Models/dino.obj");//"../../VulkanPratice/Models/dino.obj");
	pxr::UsdPrim dinoPrim = testPointer->GetPrimAtPath(pxr::SdfPath("/dino"));
	pxr::UsdGeomXformable dinoXform = pxr::UsdGeomXformable(dinoPrim);
	pxr::UsdGeomXformOp rot = dinoXform.AddRotateXOp(pxr::UsdGeomXformOp::PrecisionFloat
		,pxr::TfToken("X_Rotation"));
	rot.Set(90.0f);
	//	testRotResult.Set(90);
	std::cout << "--Opened dino.obj!--\n";
	testPointer->Export("dinoOut.usd");//"../../VulkanPratice/Models/dino.usd");
	std::cout << "--Exported dino.obj!--\n";
	testPointer->Save();
	std::cout << "--Saved dino.obj!--\n";
}