#include "Model.h"
#include <tiny_obj_loader.h>
//Load Model should go here!

    //This is more like a resource function. Move it to a resource manager when that is made
void Model::loadModel(std::string modelPath, std::string materialPath,std::string texturePath) {
    tinyobj::attrib_t attrib; //Contains positions normals texture coords
    std::vector<tinyobj::shape_t> shapes; //seperate objects and faces
    std::vector<tinyobj::material_t> localMaterials;
    std::string warn, err;
    Mesh modelMesh;
    referenceMesh = &modelMesh;
    referencePipeline = &resourceManager.pipelineList[0];
    if (!tinyobj::LoadObj(&attrib, &shapes, &localMaterials, &warn, &err, modelPath.c_str(), materialPath.c_str())) {
        throw std::runtime_error(warn + err);
    }
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    //Going to combine all the faces into to one model
    for (const auto& shape : shapes) {
        modelMesh.primativeCount += shape.mesh.num_face_vertices.size();
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
                uniqueVertices[vertex] = static_cast<uint32_t>(modelMesh.vertices.size());
                modelMesh.vertices.push_back(vertex);
            }
            modelMesh.indices.push_back(uniqueVertices[vertex]);

            //vertices.push_back(vertex);
            //indices.push_back(indices.size());

        }
        for (int matIndex : shape.mesh.material_ids) {
            modelMesh.materialIndices.push_back(matIndex);
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
        referenceMaterial = &m;
        referenceMaterialIndex = modelMesh.materials.size();
        modelMesh.materials.push_back(m);
    }

    referenceMeshIndex = resourceManager.meshList.size();
    resourceManager.meshList.push_back(modelMesh);
    resourceManager.modelList.push_back(this);
    //Load Texture
    Texture texture;
    texture.loadTexture(texturePath, resourceManager.device, resourceManager.physicalDevice);
    referenceTexture = &texture;
    resourceManager.textureList.push_back(texture);
    //Create Buffers
    modelMesh.createVertexBuffer();
    modelMesh.createIndexBuffer();
    modelMesh.createMaterialBuffer();
    modelMesh.createMaterialIndexBuffer();
}