#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

struct Material {
    vec3 specular;
    float ambient;
    float shiny;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float itensity;
};

struct ProjectorLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float itensity;
    float angle;
    float outer_angle;

};

layout(std140, set = 0, binding = 0) uniform FrameUniform {
    mat4 view;
    mat4 perspective;
    vec3 camera_pos;
    vec3 light_direction;
    vec3 light_color;
} frame_uniforms;

layout(std140, set = 1, binding = 0) uniform ObjectUniform {
    mat4 model;
    vec3 color_mult;
    Material material;
} object_uniforms;

layout(std430, set = 0, binding = 1) readonly buffer PointLightBuffer {
    uint size;
    PointLight lights[];
} pl;

layout(std430, set = 0, binding = 2) readonly buffer ProjLightBuffer {
    uint size;
    ProjectorLight lights[];
} proj;

vec3 applyAmbient(vec3 N, vec3 V) {
    return object_uniforms.material.ambient * inColor;
}

vec3 applyDirectional(vec3 N, vec3 V) {
    vec3 L = normalize(-frame_uniforms.light_direction);
    vec3 R = reflect(-L, N);
    vec3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    vec3 diff = vec3(0, 0, 0);
    vec3 spec = vec3(0, 0, 0);

    if (NdotL > 0) {
        float diff_multiplier = NdotL;
        diff = diff_multiplier * frame_uniforms.light_color * inColor;
        vec3 spec_mult = object_uniforms.material.specular * pow(NdotH, object_uniforms.material.shiny);
        spec = spec_mult * frame_uniforms.light_color;
    }

    return diff + spec;

}

vec3 applyPoint(vec3 N, vec3 V) {
    vec3 diff = vec3(0, 0, 0);
    vec3 spec = vec3(0, 0, 0);


    for(uint i = 0; i < pl.size; ++i) {
        vec3 light_pos = pl.lights[i].position;
        vec3 light_color = pl.lights[i].color;
        float color_itensity = pl.lights[i].itensity;

        vec3 L_non_normal = light_pos - inWorldPos;

        vec3 L = normalize(L_non_normal);
        vec3 H = normalize(L + V);

        float L_length = length(L_non_normal);

        if (color_itensity * L_length < 1e-5) {
            continue;
        }

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        if (NdotL > 0) {
            float diff_multiplier = NdotL * color_itensity / (L_length * L_length);
            diff += diff_multiplier * light_color * inColor;
            vec3 spec_mult = object_uniforms.material.specular * pow(NdotH, object_uniforms.material.shiny) * color_itensity / (L_length * L_length);
            spec += spec_mult * light_color;
        }
    }

    return diff + spec;
}

vec3 applyProject(vec3 N, vec3 V) {
    vec3 diff = vec3(0, 0, 0);
    vec3 spec = vec3(0, 0, 0);


    for(uint i = 0; i < proj.size; ++i) {
        vec3 light_pos = proj.lights[i].position;
        vec3 light_dir = normalize(proj.lights[i].direction);
        vec3 light_color = proj.lights[i].color;
        float color_itensity = proj.lights[i].itensity;
        float angle = proj.lights[i].angle;
        float outer_angle = proj.lights[i].outer_angle;

        vec3 L_non_normal = light_pos - inWorldPos;

        vec3 L = normalize(L_non_normal);
        vec3 H = normalize(L + V);

        float L_length = length(L_non_normal);

        if (color_itensity * L_length < 1e-5) {
            continue;
        }

        float cos_angle = cos(angle);
        float cos_outer_angle = cos(outer_angle);
        float cos_point = -dot(L, light_dir);

        float fall_off_edge = 1.0f;

        if (cos_angle > 0 && cos_outer_angle > 0 && cos_angle > cos_outer_angle) {

            if (cos_point < cos_outer_angle) {
                continue;
            } 
            if (cos_point < cos_angle) {
                fall_off_edge = (cos_point - cos_outer_angle) / (cos_angle - cos_outer_angle);
                fall_off_edge = clamp(fall_off_edge, 0.0, 1.0);
            }
        } else {
            continue; // wrong projector
        }

        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        if (NdotL > 0) {
            float diff_multiplier = fall_off_edge * NdotL * color_itensity / (L_length * L_length);
            diff += diff_multiplier * light_color * inColor;
            vec3 spec_mult = fall_off_edge * object_uniforms.material.specular * pow(NdotH, object_uniforms.material.shiny) * color_itensity / (L_length * L_length);
            spec += spec_mult * light_color;
        }
    }

    return diff + spec;

}

void main() {
    vec3 N = normalize(inNorm);
    vec3 V = normalize(frame_uniforms.camera_pos - inWorldPos);
    outColor = vec4(applyAmbient(N, V) + applyDirectional(N, V) + applyPoint(N, V) + applyProject(N, V), 1);
}