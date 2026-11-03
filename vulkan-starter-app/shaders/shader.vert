#version 450

struct Material {
    float ambient;
    float specular;
    float shiny;
};

layout(location = 0) out vec3 outColour;
layout(location = 1) out vec3 outNorm;
layout(location = 2) out vec3 outWorldPos;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNorm;

layout(std140, set = 0, binding = 0) uniform FrameUniform {
    mat4 view;
    mat4 perspective;
    vec3 camera_pos;
    vec3 direction_light;
    vec3 color_light;
} frame_uniforms;

layout(std140, set = 1, binding = 0) uniform ObjectUniform {
    mat4 model;
    vec3 color_mult;
    Material material;
} object_uniforms;

void main() {
    vec4 global_position = object_uniforms.model * vec4(inPos, 1);

    mat3 mat_for_norm = mat3(transpose(inverse(object_uniforms.model)));
    outNorm = mat_for_norm * inNorm;

    outColour = inColor * object_uniforms.color_mult;
    outWorldPos = vec3(global_position);


    gl_Position = frame_uniforms.perspective * frame_uniforms.view * global_position;

}