#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitP;
layout(binding = 2) buffer VertexBuffer { float data[];} vertexBuffer;
layout(binding = 3) buffer IndexBuffer { uint data[];} indexBuffer;

layout(push_constant) uniform _PushConstantRay {PushConstantRay pcRay;};
hitAttributeEXT vec3 attribs;

void main()
{
//Triangle Properties
    ivec3 indices = ivec3(indexBuffer.data[3 * gl_PrimitiveID + 0],
                        indexBuffer.data[3 * gl_PrimitiveID + 1],
                        indexBuffer.data[3 * gl_PrimitiveID + 2]);
    
    vec3 vertexA = vec3(vertexBuffer.data[3 * indices.x + 0],
                      vertexBuffer.data[3 * indices.x + 1],
                      vertexBuffer.data[3 * indices.x + 2]);
    vec3 vertexB = vec3(vertexBuffer.data[3 * indices.y + 0],
                      vertexBuffer.data[3 * indices.y + 1],
                      vertexBuffer.data[3 * indices.y + 2]);
    vec3 vertexC = vec3(vertexBuffer.data[3 * indices.z + 0],
                      vertexBuffer.data[3 * indices.z + 1],
                      vertexBuffer.data[3 * indices.z + 2]);
	vec3 barycentric = vec3(1.0 - attribs.x - attribs.y,
                          attribs.x, attribs.y);
	//Derived Properties
    vec3 testHit = vertexA * barycentric.x + vertexB * barycentric.y +
                  vertexC * barycentric.z;
    testHit = vec3(testHit * gl_ObjectToWorldEXT);
    vec3 normal = normalize(cross(vertexB - vertexA,vertexC-vertexA));
    vec3 worldNormal = normalize(vec3(normal * gl_ObjectToWorldEXT)); 
	vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 lightDir = pcRay.lightPos - worldPos;
    float l = dot(lightDir,worldNormal);
	float d = length(worldPos)/5.0; 
	hitP.hitValue = vec3(normal.x,0,0);//vec3(testHit.x,testHit.y,testHit.z);
}
