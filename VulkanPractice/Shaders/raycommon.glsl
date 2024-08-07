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
    int rayDepth;
    uint rngState;
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

float rand(inout uint v)
{
	uint state = v * 747796405u + 2891336453u;
    v = state;
	uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
	uint result = (word >> 22u) ^ word;
    return (float(result)/ float(uint(0xffffffff)));
}

vec3 randomUnitVector(inout uint v){
    uint tmpV = v;
    float x = rand(tmpV);
    float y = rand(tmpV);
    float z = rand(tmpV);
    vec3 outVector;
    outVector = vec3(2.0*x-1.0,2.0*y-1.0,2.0*z-1.0);
    outVector = normalize(outVector);
    v = tmpV;
    return outVector;
}

vec3 randomHemisphereVector(inout uint v,vec3 normal){
    vec3 unitVector = randomUnitVector(v);
    float d = dot(normal,unitVector);
    if(d < 0) unitVector = -unitVector;
    return unitVector;
}

float linear_to_gamma(float linear){
    float value = 0.0;
    if(linear > 0.0) value = sqrt(linear);
    return value;
}
//Schlick reflectance approx
float reflectance(float cosine, float ior){
    float r0 = (1 - ior) / (1 + ior);
    r0 = r0*r0;
    return r0 + (1-r0)*pow((1 - cosine),5);
}
#endif
