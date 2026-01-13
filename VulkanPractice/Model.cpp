#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "common.h"
#include "ResourceManager.h"
#include "Pipeline.h"
#include "Mesh.h"
#include "Model.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformOp.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
//Load Model should go here!

    //This is more like a resource function. Move it to a resource manager when that is made
void Model::loadModel(std::string modelPath, std::string materialPath,std::string texturePath) {
    tinyobj::attrib_t attrib; //Contains positions normals texture coords
    std::vector<tinyobj::shape_t> shapes; //seperate objects and faces
    std::vector<tinyobj::material_t> localMaterials;
    std::string warn, err;
    modelMatrix = glm::identity<glm::mat4x4>();
    Mesh initMesh;
    ResourceManager::manager->meshList.push_back(initMesh);
    int endIndex = ResourceManager::manager->meshList.size()-1;
    Mesh* modelMesh = &ResourceManager::manager->meshList[endIndex];
    modelMesh->resourceListIndex = endIndex;
    resourceListIndex = ResourceManager::manager->modelList.size();
    referenceMeshIndex = endIndex;
    referencePipelineIndex = 0;
    allocatedDescSetIndex = ResourceManager::manager->pipelineList[referencePipelineIndex].allocatedSets*2;
    ResourceManager::manager->pipelineList[referencePipelineIndex].allocatedSets++;
    if (!tinyobj::LoadObj(&attrib, &shapes, &localMaterials, &warn, &err, modelPath.c_str(), materialPath.c_str())) {
        throw std::runtime_error(warn + err);
    }
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    //Going to combine all the faces into to one model
    for (const auto& shape : shapes) {
        modelMesh->primativeCount += shape.mesh.num_face_vertices.size();
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            //array of 'vec3' represented as floats only, hence the 3 *
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (!attrib.normals.empty() && index.normal_index > 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            //array of 'vec2' represented as floats only, hence the 3 *
            //Invert the y axis because of obj format
            if (!attrib.texcoords.empty() && index.texcoord_index > 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            vertex.color = { 1.0f, 1.0f, 1.0f };


            //Load in the indcies and vertices
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(modelMesh->vertices.size());
                modelMesh->vertices.push_back(vertex);
            }
            modelMesh->indices.push_back(uniqueVertices[vertex]);

            //vertices.push_back(vertex);
            //indices.push_back(indices.size());

        }
        for (int matIndex : shape.mesh.material_ids) {
            modelMesh->materialIndices.push_back(matIndex);
        }
    }
    //Copy material infomation into vector
    for (uint32_t x = 0; x < localMaterials.size(); x++) {
        Material m;
        m.ambient = float3ToVec4(localMaterials[x].ambient);
        m.diffuse = float3ToVec4(localMaterials[x].diffuse);
        //Using IOR Is stored in ambient
        m.ambient.x = localMaterials[x].ior;
        m.ambient.y = localMaterials[x].illum;
        float clampProb = localMaterials[x].ambient[0];
        if (clampProb < 0) clampProb = 0.0;
        if (clampProb > 1.0) clampProb = 1.0;
        m.diffuse.a = clampProb;
        m.specular = float3ToVec4(localMaterials[x].specular);
        float clampedShininess = localMaterials[x].shininess;
        //Use shininess as a blending value for reflective surfaces
        if (clampedShininess < 0) clampedShininess = 0.0;
        if (clampedShininess > 1.0) clampedShininess = 1.0;
        m.specular.a = clampedShininess;
        m.emission = float3ToVec4(localMaterials[x].emission);
        modelMesh->materials.push_back(m);
    }

    referenceMeshIndex = ResourceManager::manager->meshList.size()-1;
    modelMesh->vertexCount = modelMesh->vertices.size();
    modelMesh->indexCount = modelMesh->indices.size();
    //Load Texture
    Texture texture;
    bool hasLoaded = false;
    hasLoaded = texture.loadTexture(texturePath, ResourceManager::manager->device, ResourceManager::manager->physicalDevice);
    if(hasLoaded){
        ResourceManager::manager->textureList.push_back(texture);
        int textEndIndex = ResourceManager::manager->textureList.size() - 1;
        referenceTextureIndex = textEndIndex;
    }
    //Create Buffers
    modelMesh->createVertexBuffer();
    modelMesh->createIndexBuffer();
    //Create material buffers if materials exist
    if (modelMesh->materials.size() > 0) {
        modelMesh->createMaterialBuffer();
        modelMesh->createMaterialIndexBuffer();
    }
    createUniformBuffers();
}

void Model::loadModel(pxr::UsdPrim prim, pxr::UsdPrim matPrim) {
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
    modelMatrix = glmMat;

    //Extract Mesh Info
    pxr::UsdPrim meshPrim;
    bool meshFound = false;
    for (pxr::UsdPrim prim : prim.GetAllChildren()) {
        if (prim.IsA<pxr::UsdGeomMesh>()) {
            meshPrim = prim;
            meshFound = true;
            break;
        }
    }
    if (!meshFound) {
        std::cout << "Mesh not found\n";
        return;
    }
    Mesh initMesh;
    ResourceManager::manager->meshList.push_back(initMesh);
    int endIndex = ResourceManager::manager->meshList.size() - 1;
    Mesh* modelMesh = &ResourceManager::manager->meshList[endIndex];
    modelMesh->resourceListIndex = endIndex;
    resourceListIndex = ResourceManager::manager->modelList.size();
    referenceMeshIndex = endIndex;
    referencePipelineIndex = 0;
    allocatedDescSetIndex = ResourceManager::manager->pipelineList[referencePipelineIndex].allocatedSets * 2;
    ResourceManager::manager->pipelineList[referencePipelineIndex].allocatedSets++;

    pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh(meshPrim);
    pxr::UsdShadeMaterialBindingAPI meshMatBindApi = pxr::UsdShadeMaterialBindingAPI(meshPrim);
    pxr::UsdShadeMaterial mat;
    bool hasMatBinding = false;
    if (meshPrim.HasAPI(pxr::TfToken("MaterialBindingAPI"))) {
        mat = meshMatBindApi.ComputeBoundMaterial();
        hasMatBinding = true;
    }
    pxr::UsdShadeShader image;
    pxr::UsdShadeShader bsdfValues;
    pxr::UsdShadeInput inputFile;
    if (hasMatBinding) {
        Material m;
        pxr::UsdPrim matPrim = mat.GetPrim();
        pxr::UsdPrim imagePrim;// = matPrim.GetPrimAtPath(pxr::SdfPath("/Image_Texture"));
        pxr::UsdPrim bsdfPrim;
        for (pxr::UsdPrim prim : matPrim.GetAllChildren()) {
            if (prim.IsA<pxr::UsdShadeShader>() && prim.GetName() == "Image_Texture") {
                imagePrim = prim;
            }
            if (prim.IsA<pxr::UsdShadeShader>() && prim.GetName() == "Principled_BSDF") {
                bsdfPrim = prim;
            }
        }
        image = pxr::UsdShadeShader(imagePrim);
        bsdfValues = pxr::UsdShadeShader(bsdfPrim);
        inputFile = image.GetInput(pxr::TfToken("file"));
        pxr::SdfAssetPath path;
        inputFile.Get(&path);
        std::cout << "\nMat Texture File Path: " << path << " | " << inputFile.GetFullName().GetString();
        //Load Texture
        Texture texture;
        bool hasLoaded = false;
        hasLoaded = texture.loadTexture(path.GetAssetPath(), ResourceManager::manager->device, ResourceManager::manager->physicalDevice);
        if (hasLoaded) {
            ResourceManager::manager->textureList.push_back(texture);
            int textEndIndex = ResourceManager::manager->textureList.size() - 1;
            referenceTextureIndex = textEndIndex;
        }


        float ior;
        float metallic;
        float opacity;
        float roughness;
        float specular;
        ior = loadMatValue<float>(bsdfValues, "ior");
        metallic = loadMatValue<float>(bsdfValues, "metallic");
        opacity = loadMatValue<float>(bsdfValues, "opacity");
        roughness = loadMatValue<float>(bsdfValues, "roughness");
        specular = loadMatValue<float>(bsdfValues, "specular");
        
        //TODO: Ambient, emission, and diffuse need values
        m.ambient = glm::vec4(0,0,0,0);
        m.diffuse = glm::vec4(0, 0, 0, 0);
        //Using IOR Is stored in ambient
        m.ambient.x = ior;
        m.ambient.y = metallic;
        float clampProb = m.ambient.x;
        if (clampProb < 0) clampProb = 0.0;
        if (clampProb > 1.0) clampProb = 1.0;
        m.diffuse.a = clampProb;
        m.specular = glm::vec4(specular, specular, specular, specular);
        float clampedShininess = roughness;
        //Use shininess as a blending value for reflective surfaces
        if (clampedShininess < 0) clampedShininess = 0.0;
        if (clampedShininess > 1.0) clampedShininess = 1.0;
        m.specular.a = clampedShininess;
        m.emission = glm::vec4(0, 0, 0, 0);
        std::cout << "\nMat Values ";
        std::cout << "IOR: " << ior;
        std::cout << " metallic: " << metallic;
        std::cout << " opacity: " << opacity;
        std::cout << " roughness: " << roughness;
        std::cout << " specular: " << specular << "\n";
        modelMesh->materials.push_back(m);
        modelMesh->materialIndices.push_back(0);
    }


    pxr::UsdAttribute pointAttr = mesh.GetPointsAttr();
    pxr::UsdGeomPrimvarsAPI meshPrimvars = pxr::UsdGeomPrimvarsAPI(meshPrim);
    //Retrive data from meshPrimvars API var (UVMap is the name for uv coords)
    pxr::UsdGeomPrimvar meshUVMapvar = meshPrimvars.GetPrimvar(pxr::TfToken("UVMap"));
    pxr::UsdAttribute normalAttr = mesh.GetNormalsAttr();
    pxr::UsdAttribute triIndicesAttr = mesh.GetFaceVertexIndicesAttr();
    pxr::UsdAttribute triFaceCountAttr = mesh.GetFaceVertexCountsAttr();

    pxr::VtArray<pxr::GfVec2f> uvArray = pxr::VtArray<pxr::GfVec2f>();
    pxr::VtArray<pxr::GfVec3f> normalArray = pxr::VtArray<pxr::GfVec3f>();
    pxr::VtArray <int> triIndexArray = pxr::VtArray<int>();
    pxr::VtArray <int> faceCountArray = pxr::VtArray<int>();
    pxr::VtArray<pxr::GfVec3f> pointArray = pxr::VtArray<pxr::GfVec3f>();

    //TODO: Copy array data into empty mesh
    bool gotUvs = meshUVMapvar.Get(&uvArray);
    bool gotNormals = normalAttr.Get(&normalArray);
    bool gotTriIndices = triIndicesAttr.Get(&triIndexArray);
    bool gotFaceCounts = triFaceCountAttr.Get(&faceCountArray);
    bool gotPoints = pointAttr.Get(&pointArray);
    if (gotPoints) {
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};
        for (int i = 0; i < faceCountArray.size(); i++) {
            if(faceCountArray[i] == 3)modelMesh->primativeCount += 1;
            else modelMesh->primativeCount += 2;
        }
        int faceIndex = 0;
        int faceVertCount = faceCountArray[faceIndex];
        for (int i = 0; i < triIndexArray.size(); i+=faceVertCount) {
            //If it is a quad, we need to trianglate it
            if (faceVertCount == 4) {
                modelMesh->indices.push_back(triIndexArray[i + 2]);
                modelMesh->indices.push_back(triIndexArray[i + 1]);
                modelMesh->indices.push_back(triIndexArray[i]);

                modelMesh->indices.push_back(triIndexArray[i + 3]);
                modelMesh->indices.push_back(triIndexArray[i + 2]);
                modelMesh->indices.push_back(triIndexArray[i]);
            }
            else {
                modelMesh->indices.push_back(triIndexArray[i + 2]);
                modelMesh->indices.push_back(triIndexArray[i + 1]);
                modelMesh->indices.push_back(triIndexArray[i]);
            }         
            if(faceIndex + 1 < faceCountArray.size())faceVertCount = faceCountArray[faceIndex++];
        }
        for (int i = 0; i < pointArray.size(); i++) {
            Vertex vertex{};
            vertex.pos = {
                pointArray[i][0],
                    pointArray[i][1],
                    pointArray[i][2]
            };
            vertex.normal = {
                normalArray[i][0],
                    normalArray[i][1],
                    normalArray[i][2]
            };
            vertex.texCoord = {
                    uvArray[i][0],
                    1.0f - uvArray[i][1]
            };
            modelMesh->vertices.push_back(vertex);
        }
        /*
        for (int i = 0; i < triIndexArray.size(); i++) {
            Vertex vertex{};
            vertex.pos = {
                pointArray[triIndexArray[i]][0],
                    pointArray[triIndexArray[i]][1],
                    pointArray[triIndexArray[i]][2]
            };
            vertex.normal = {
                normalArray[triIndexArray[i]][0],
                    normalArray[triIndexArray[i]][1],
                    normalArray[triIndexArray[i]][2]
            };
            vertex.texCoord = {
                    uvArray[triIndexArray[i]][0],
                    1.0f - uvArray[triIndexArray[i]][1]
            };
            //Load in the indcies and vertices
            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(modelMesh->vertices.size());
                modelMesh->vertices.push_back(vertex);
            }
            modelMesh->indices.push_back(uniqueVertices[vertex]);
        }
        */
    }
    else std::cout << "FAIL" << "\n";
    if (gotNormals) std::cout << "NORMALS SUCCESS" << "\n";
    else std::cout << "FAIL" << "\n";
    if (gotTriIndices) std::cout << "TRI INDICES SUCCESS" << "\n";
    else std::cout << "FAIL" << "\n";
    if (gotUvs) std::cout << "UVS SUCCESS" << "\n";
    else std::cout << "FAIL" << "\n";

    referenceMeshIndex = ResourceManager::manager->meshList.size() - 1;
    modelMesh->vertexCount = modelMesh->vertices.size();
    modelMesh->indexCount = modelMesh->indices.size();
    //Create Buffers
    modelMesh->createVertexBuffer();
    modelMesh->createIndexBuffer();
    //Create material buffers if materials exist
    if (modelMesh->materials.size() > 0) {
        modelMesh->createMaterialBuffer();
        modelMesh->createMaterialIndexBuffer();
    }
    createUniformBuffers();
}

void Model::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    //Create buffers for the frames that are being worked on in parallel
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        ResourceManager::manager->createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]
            , ResourceManager::manager->useRayTracing);
#ifndef NDEBUG
        ResourceManager::setDebugObjectName(ResourceManager::manager->device, VkObjectType::VK_OBJECT_TYPE_BUFFER, reinterpret_cast<uint64_t>(uniformBuffers[i]), "Model Uniform Buffer Object " + i);
#endif
    }
}
void Model::updateUniformBuffers(uint32_t frameNum) {
    void* data;
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
    UniformBufferObject ubo{};
    //We create an indentity matrix and rotate based on the time
    ubo.model = modelMatrix;
    ubo.view = ResourceManager::manager->mainCamera.view;
    ubo.proj = ResourceManager::manager->mainCamera.proj;
    vkMapMemory(ResourceManager::manager->device, uniformBuffersMemory[frameNum], 0
        , bufferSize, 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(ResourceManager::manager->device, uniformBuffersMemory[frameNum]);
}
//Transforms
void Model::setScale(glm::vec3 scale) {
    glm::vec3 oldScale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(modelMatrix, oldScale, rotation, translation, skew, perspective);
    oldScale = scale;
    glm::mat4 translateMat = glm::translate(glm::mat4(1.0), translation);
    glm::mat4 rotateMat = glm::mat4_cast(rotation);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), oldScale);

    modelMatrix = translateMat * rotateMat * scaleMat;
}
void Model::setPosition(glm::vec3 pos) {
    modelMatrix[3][0] = pos.x;
    modelMatrix[3][1] = pos.y;
    modelMatrix[3][2] = pos.z;
}
//Sets rotation using eulerAngles (in Radians)
void Model::setRotation(glm::vec3 eulerAngles) {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(modelMatrix, scale, rotation, translation, skew,perspective);
    rotation = glm::quat(eulerAngles);
    glm::mat4 translateMat = glm::translate(glm::mat4(1.0), translation);
    glm::mat4 rotateMat = glm::mat4_cast(rotation);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), scale);

    modelMatrix  = translateMat * rotateMat * scaleMat;
}
void Model::rotateInPlace(glm::vec3 deltaAngles) {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(modelMatrix, scale, rotation, translation, skew, perspective);
    rotation *= glm::quat(deltaAngles);
    glm::mat4 translateMat = glm::translate(glm::mat4(1.0), translation);
    glm::mat4 rotateMat = glm::mat4_cast(rotation);
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0), scale);

    modelMatrix = translateMat * rotateMat * scaleMat;
}
static float totalT = 0;
void Model::testUpdate() {
    float delta = ResourceManager::manager->deltaTime;
    totalT += delta;
    rotateInPlace(glm::vec3(0.0, delta, 0.0));
    //setPosition(glm::vec3(0.0, 10.0 * sin(totalT),0.0));
   // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    //modelMatrix = glm::rotate(modelMatrix, delta * glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
}