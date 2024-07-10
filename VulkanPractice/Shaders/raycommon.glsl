#ifndef RAY_COMMON_GLSL
#define RAY_COMMON_GLSL

//Hit payload
struct HitPayload{
    vec3 hitValue;
};
struct PushConstantRay
{
	vec4 clearColor;
	vec3 lightPos;
	float lightIntensity;
	int lightType;
};
struct Vertex {
    vec3 pos;
    vec3 color;
    vec2 texCoord;
};
#endif
