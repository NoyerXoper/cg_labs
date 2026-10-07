#version 450

layout(location = 0) out vec3 outColour;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(std140, set = 0, binding = 0) uniform GlobalUniform {
    mat4 matrix;
    vec3 color_mult;
} global_uniforms;

void main() {
    gl_Position = global_uniforms.matrix * vec4(inPos, 1);
    outColour = vec3(
        inColor.x * global_uniforms.color_mult.x,
        inColor.y * global_uniforms.color_mult.y,
        inColor.z * global_uniforms.color_mult.z
    );
}