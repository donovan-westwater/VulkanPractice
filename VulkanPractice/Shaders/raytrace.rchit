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
    uint matIndex = materialIndexBuffer.data[gl_PrimitiveID];
    vec3 hitcolor = materialBuffer.data[matIndex].diffuse.xyz;
    vec3 specColor = materialBuffer.data[matIndex].specular.xyz;
    float shininess = materialBuffer.data[matIndex].specular.w;
    float specProb = materialBuffer.data[matIndex].diffuse.w;
    float ior = materialBuffer.data[matIndex].ambient.x;
    float ill = materialBuffer.data[matIndex].ambient.y;
    //Send new ray
    //Set flags to describe the geometry being dealt with
    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.001;
    float tMax = 10000.0;
    vec3 dir = gl_WorldRayDirectionEXT;
    uint state = hitP.rngState;
    vec3 rayDirection = vec3(0,0,0);
    if(ill == 4){
        //Dieletric response - Handles refraction through clear materials
        float testDot = dot(dir, worldNormal);
        //are we entering or leaving the material?
	    vec3 outwardNormal = testDot > 0 ? -worldNormal : worldNormal;
	    float niOverNt = testDot > 0 ? ior : 1 / ior;
	    float cosine = testDot > 0 ? ior * testDot : -testDot;
        float sin_theta = sqrt(1-cosine*cosine);
        //Light reflects iternally at glancing angles
        if(sin_theta*ior > 1.0 || reflectance(cosine,ior) > rand(state)){
            rayDirection = reflect(dir,outwardNormal);
        //Otherwise we refract the light traveling through
        }else{
            rayDirection  = refract(dir,outwardNormal,niOverNt);
        }
    }else{
        //Diffuse Direction Calculation
        rayDirection = randomUnitVector(state)+worldNormal;
        if(length(rayDirection) < 0.0001) rayDirection = worldNormal;
        rayDirection = normalize(rayDirection);
        //Specular Direction Calculation
        vec3 specReflectDir = gl_WorldRayDirectionEXT - worldNormal*dot(gl_WorldRayDirectionEXT,worldNormal)*2;
        float doSpec = rand(state) <= specProb ? 1.0 : 0.0;
        //Final Direction Result
        vec3 blendDir = normalize(mix(rayDirection,specReflectDir,shininess*shininess));
        vec3 finalDir = mix(rayDirection,blendDir,doSpec);
            rayDirection = finalDir;
        vec3 finalColor = mix(hitcolor.xyz,specColor.xyz,doSpec);
        hitP.hitValue *= 0.5*finalColor;
    }
    //Skipping Emission for now until noise is better dealt with
    //hitP.hitValue += materialBuffer.data[matIndex].emission.xyz;
    hitP.rngState = state;
    //TO DO: Should pass in max depth from CPU side. Pipeline controls depth!
    //Glossy Step: Goaling to coopt Ni parameter as a specular probablity and use that method for glossy
    //Based off this: https://blog.demofox.org/2020/06/06/casual-shadertoy-path-tracing-2-image-improvement-and-glossy-reflections/
    //I'll attempt physically based stuff later once the foundation is more rock solid
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
