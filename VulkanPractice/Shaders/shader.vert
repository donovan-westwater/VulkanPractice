#version 450
//These are vertex attributes. Layout corrsponds to indices setup in the vertex part of the pipeline
//NOTE: dvec3 are 64 bit vectors which means they use MULTIPLE SLOTS! next index must be >=2 higher
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
//This should corrispond to indices in the frag section of pipeline
layout(location = 0) out vec3 fragColor;
//invoked for every vertex
void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}