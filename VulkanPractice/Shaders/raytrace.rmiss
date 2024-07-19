#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitP;

void main()
{
    if(hitP.rayDepth < 1)hitP.hitValue = vec3(0.0, 1.0, 1.0);
    hitP.rayDepth = 100;
}