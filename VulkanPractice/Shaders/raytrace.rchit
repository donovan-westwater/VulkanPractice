#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload hitP;
//hitAttributeEXT vec3 attribs;

void main()
{
	vec3 worldPos = gl_WorldRayDirectionEXT * gl_HitTEXT;
	float d = length(worldPos)/1000.0;
	hitP.hitValue = vec3(d,d,d);
}
