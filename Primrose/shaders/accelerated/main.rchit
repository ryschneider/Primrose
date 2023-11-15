#version 460 core
#extension GL_EXT_ray_tracing : require

#include "payload.glsl"
layout(location = 0) rayPayloadInEXT Payload payload;

hitAttributeEXT vec3 normal;

void main() {
	payload.color = abs(normal);
}
