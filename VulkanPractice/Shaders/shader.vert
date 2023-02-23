#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    //vec4 colorAdd;
} ubo;
//These are vertex attributes. Layout corrsponds to indices setup in the vertex part of the pipeline
//NOTE: dvec3 are 64 bit vectors which means they use MULTIPLE SLOTS! next index must be >=2 higher
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
//This should corrispond to indices in the frag section of pipeline
layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
//invoked for every vertex
void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;//+ubo.colorAdd.xyz*ubo.colorAdd.w;
    fragTexCoord = inTexCoord;
}