#version 450

layout(location = 0) in vec2 screenXY;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform GlobalUniforms {
    vec3 camPos;
    vec3 camDir;
    vec3 camUp;
    float screenHeight;

    float focalLength;
    float invZoom;
} u;

layout(push_constant) uniform PushConstant {
    float time;
} p;

// main
void main() {
//    fragColor = vec4(sin(screenXY), sin(p.time), sin(p.time) > 0 ? 1.f : 0.f);
    if (length(screenXY) > 0.1) {
        fragColor = vec4(0.f, 0.f, 0.f, 1.f);
    } else {
        fragColor = vec4(0.f);
    }
}
