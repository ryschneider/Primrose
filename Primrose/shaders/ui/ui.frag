#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D texSampler;

const uint UI_IMAGE = 501;
const uint UI_PANEL = 502;
const uint UI_TEXT = 503;

layout(push_constant) uniform PushConstant {
    uint uiType;
    vec4 color;
    vec4 borderColor;
    float borderStroke;
} p;

// main
void main() {
    if (p.uiType == UI_IMAGE) {
        fragColor = texture(texSampler, uv);
    } else if (p.uiType == UI_TEXT) {
        float v = texture(texSampler, uv).r;
        fragColor = vec4(vec3(1), v);
    } else if (p.uiType == UI_PANEL) {
        vec2 rel = abs(0.5 - uv); // distance from centre
        if (max(rel.x, rel.y) > 0.5 - p.borderStroke) fragColor = p.borderColor;
        else fragColor = p.color;
    } else {
        fragColor = vec4(1.f, 0.f, 0.f, 1.f);
    }
}
