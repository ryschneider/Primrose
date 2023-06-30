#version 450

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D tex;

// main
void main() {
    if (gl_FragCoord.x > 0) {
        fragColor = vec4(uv, 0.f, 1.f);
    }
//    if (length(screenXY) > 1.2) {
//        fragColor = vec4(0.f, 0.f, 0.f, 1.f);
//    } else {
//        fragColor = vec4(0.f);
//    }
}
