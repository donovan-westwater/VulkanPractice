#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitP;
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
	float d = length(worldPos)/5.0; 
	hitP.hitValue = materialBuffer.data[0].diffuse.xyz*vec3(l,l,l);//abs(vec3(worldNormal.x,worldNormal.y,worldNormal.z));
}
