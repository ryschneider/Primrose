#version 460 core
#extension GL_EXT_ray_tracing : require

#include "payload.glsl"
layout(location = 0) rayPayloadInEXT Payload payload;

void main() {
	payload.color = vec3(1, 0, 0);
}
