#version 450

layout(location = 0) in vec2 screenXY;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform GlobalUniforms {
	float time;
	float screenHeight;
	vec3 camPos;
	vec3 camDir;
} u;

// main
void main() {
	fragColor = vec4(sin(screenXY), sin(u.time), 1.f);
}
