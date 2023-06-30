#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D texSampler;

// main
void main() {
    fragColor = texture(texSampler, uv);
}
