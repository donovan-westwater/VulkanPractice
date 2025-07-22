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
#include <glm/gtx/matrix_decompose.hpp>

void LevelManager::testImportAndExport() {
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
	rot.Set(270.0f);
	//	testRotResult.Set(90);
	std::cout << "--Opened dino.obj!--\n";
	testPointer->Export("dinoOut.usd");//"../../VulkanPratice/Models/dino.usd");
	std::cout << "--Exported dino.obj!--\n";
	testPointer->Save();
	std::cout << "--Saved dino.obj!--\n";
}
void LevelManager::loadPrim(pxr::UsdPrim prim) {
	pxr::UsdGeomXformable primXform = pxr::UsdGeomXformable(prim);
	pxr::GfMatrix4d pxrMat = primXform.GetTransformOp().GetOpTransform(pxr::UsdTimeCode(0.0));
	double* pxrMatArray = pxrMat.GetArray();
	glm::mat4x4 glmMat = glm::mat4x4();
	//Convert info a format that we can use!
	for (int i = 0; i < 4; i++) {
		glmMat[i] = glm::vec4(pxrMatArray[4 * i], pxrMatArray[4 * i + 1], pxrMatArray[4 * i + 2], pxrMatArray[4 * i + 3]);
	}
	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 translation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(glmMat, scale, rotation, translation, skew, perspective);
	std::cout << prim.GetName() << " POS: " << translation.x << " " << translation.y << " " << translation.z << "\n";
}
void LevelManager::loadLevel(std::string levelName) {
	testPointer = pxr::UsdStage::Open(levelName);
	pxr::UsdPrim levelPrim = testPointer->GetPrimAtPath(pxr::SdfPath("/Level"));
	for(pxr::UsdPrim prim : levelPrim.GetAllChildren()) {
		loadPrim(prim);
	}
	std::cout << "_________________\n";
	levelPrim = testPointer->GetPrimAtPath(pxr::SdfPath("/Entities"));
	for (pxr::UsdPrim prim : levelPrim.GetAllChildren()) {
		loadPrim(prim);
	}
}
