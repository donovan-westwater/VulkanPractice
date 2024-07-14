#ifndef RAY_COMMON_GLSL
#define RAY_COMMON_GLSL

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

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
	vec3 normal;
    vec3 color;
    vec2 texCoord;
};
struct Material {
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
    vec4 emission;
};
#endif
