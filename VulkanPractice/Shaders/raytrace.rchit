#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitP;
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 0, set = 1) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;
layout(binding = 2) buffer VertexBuffer { Vertex data[];} vertexBuffer; //Positions are being read wrong
layout(binding = 3) buffer IndexBuffer { uint data[];} indexBuffer;
layout(binding = 4) buffer MaterialBuffer { Material data[];} materialBuffer;
layout(binding = 5) buffer MaterialIndexBuffer { uint data[];} materialIndexBuffer;

layout(push_constant) uniform _PushConstantRay {PushConstantRay pcRay;};
hitAttributeEXT vec3 attribs;

void main()
{
    //Triangle Properties
    ivec3 indices = ivec3(indexBuffer.data[3 * gl_PrimitiveID + 0],
                        indexBuffer.data[3 * gl_PrimitiveID + 1],
                        indexBuffer.data[3 * gl_PrimitiveID + 2]);
    
    vec3 vertexA = vertexBuffer.data[indices.x].pos;
    vec3 vertexB = vertexBuffer.data[indices.y].pos;
    vec3 vertexC = vertexBuffer.data[indices.z].pos;
	vec3 barycentric = vec3(1.0 - attribs.x - attribs.y,
                          attribs.x, attribs.y);
	//Derived Properties
    vec3 normal = normalize(cross(vertexB - vertexA,vertexC  - vertexA));
    vec3 worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT)); 
	vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    
    vec3 lightDir = pcRay.lightPos - worldPos;
    float l = dot(lightDir,worldNormal);
    hitP.rayDepth += 1;
	hitP.hitValue += pow(0.5,hitP.rayDepth)*l*materialBuffer.data[gl_PrimitiveID].diffuse.xyz;
    //Send new ray
    //Set flags to describe the geometry being dealt with
    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.001;
    float tMax = 10000.0;
    vec3 rayDirection = randomHemisphereVector(vec2(.27,.62),worldNormal);
    //TO DO: Should pass in max depth from CPU side. Pipeline controls depth!
    if(hitP.rayDepth < 6){
        traceRayEXT(topLevelAS, // acceleration structure
                rayFlags,       // rayFlags
                0xFF,           // cullMask
                0,              // sbtRecordOffset
                0,              // sbtRecordStride
                0,              // missIndex
                worldPos.xyz,     // ray origin
                tMin,           // ray min range
                rayDirection,  // ray direction
                tMax,           // ray max range
                0               // payload (location = 0)
        );
    }
}
