#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "common.h"
#include "ResourceManager.h"
#include "Pipeline.h"
#include "Mesh.h"
#include "Model.h"
#include <glm/gtx/matrix_decompose.hpp>
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
    referenceMeshIndex = endIndex;
    referencePipelineIndex = 0;
    allocatedDescSetIndex = ResourceManager::manager->pipelineList[referencePipelineIndex].allocatedSets;
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
void Model::testUpdate() {
    float delta = ResourceManager::manager->deltaTime;
   // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
   // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = glm::rotate(modelMatrix, delta * glm::radians(10.0f), glm::vec3(0.0f, 0.0f, 1.0f));
}