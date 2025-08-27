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
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

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
	int meshIndex = 0;
	//Extract Mesh Info
	pxr::UsdPrim meshPrim;
	pxr::UsdPrim matPrim = testPointer->GetPrimAtPath(pxr::SdfPath("/_materials"));
	for (pxr::UsdPrim prim : prim.GetAllChildren()) {
		if (prim.IsA<pxr::UsdGeomMesh>()) {
			meshPrim = prim;
			break;
		}
	}
	pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh(meshPrim);
	pxr::UsdShadeMaterialBindingAPI meshMatBindApi = pxr::UsdShadeMaterialBindingAPI(meshPrim);
	pxr::UsdShadeMaterial mat;
	bool hasMatBinding = false;
	if (meshPrim.HasAPI(pxr::TfToken("MaterialBindingAPI"))) {
		mat = meshMatBindApi.ComputeBoundMaterial();
		hasMatBinding = true;
	}
	pxr::UsdShadeShader image;
	pxr::UsdShadeInput inputFile;
	if (hasMatBinding) {
		pxr::UsdPrim matPrim = mat.GetPrim();
		pxr::UsdPrim imagePrim;// = matPrim.GetPrimAtPath(pxr::SdfPath("/Image_Texture"));
		for (pxr::UsdPrim prim : matPrim.GetAllChildren()) {
			if (prim.IsA<pxr::UsdShadeShader>() && prim.GetName() == "Image_Texture") {
				imagePrim = prim;
				break;
			}
		}
		image = pxr::UsdShadeShader(imagePrim);
		inputFile = image.GetInput(pxr::TfToken("file"));
		pxr::SdfAssetPath path;
		inputFile.Get(&path);
		std::cout << "\nMat Texture File Path: "<<path<<" | " << inputFile.GetFullName().GetString();
	}
	pxr::UsdAttribute pointAttr = mesh.GetPointsAttr();
	pxr::UsdGeomPrimvarsAPI meshPrimvars = pxr::UsdGeomPrimvarsAPI(meshPrim);
	//Retrive data from meshPrimvars API var (UVMap is the name for uv coords)
	pxr::UsdGeomPrimvar meshUVMapvar = meshPrimvars.GetPrimvar(pxr::TfToken("UVMap"));
	pxr::UsdAttribute normalAttr = mesh.GetNormalsAttr();
	pxr::UsdAttribute triIndicesAttr = mesh.GetFaceVertexIndicesAttr();

	pxr::VtArray<pxr::GfVec2f> uvArray = pxr::VtArray<pxr::GfVec2f>();
	pxr::VtArray<pxr::GfVec3f> normalArray = pxr::VtArray<pxr::GfVec3f>();
	pxr::VtArray <int> triIndexArray = pxr::VtArray<int>();
	pxr::VtArray<pxr::GfVec3f> pointArray = pxr::VtArray<pxr::GfVec3f>();

	bool gotUvs = meshUVMapvar.Get(&uvArray);
	bool gotNormals = normalAttr.Get(&normalArray);
	bool gotTriIndices = triIndicesAttr.Get(&triIndexArray);
	bool gotPoints = pointAttr.Get(&pointArray);
	if(gotPoints) std::cout << "POINTS SUCCESS" << "\n";
	else std::cout << "FAIL" << "\n";
	if (gotNormals) std::cout << "NORMALS SUCCESS" << "\n";
	else std::cout << "FAIL" << "\n";
	if (gotTriIndices) std::cout << "TRI INDICES SUCCESS" << "\n";
	else std::cout << "FAIL" << "\n";
	if (gotUvs) std::cout << "UVS SUCCESS" << "\n";
	else std::cout << "FAIL" << "\n";

	if (hasMatBinding) std::cout << mat.GetPath().GetString()<<"\n";

	std::cout << mesh.GetFaceCount() << "\n";
	std::cout << "\nPOINTS| ";
	/*
	for (pxr::GfVec3f p : pointArray) {
		std::cout << p[0] << " " << p[1] << " " << p[2]<<" " << "\n";
	}
	std::cout << "\nUvs| ";
	for (pxr::GfVec2f p : uvArray) {
		std::cout << p[0] << " " << p[1] << " " << "\n";
	}
	std::cout << "\nNormals| ";
	for (pxr::GfVec3f p : normalArray) {
		std::cout << p[0] << " " << p[1] << " " << p[2] << " " << "\n";
	}
	std::cout << "\nTriangle Indices| ";
	for (int p : triIndexArray) {
		std::cout << p << "\n";
	}
	*/
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
