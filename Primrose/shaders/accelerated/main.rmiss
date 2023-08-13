#version 460 core
#extension GL_EXT_ray_tracing : require

#include "payload.glsl"
layout(location = 0) rayPayloadInEXT Payload payload;

const int TILE_W = 24;
const int TILE_H = 16;

void main() {
	payload.color = mix(vec3(0.f, 0.1f, 0.f), vec3(0.1f, 0.f, 0.1f),
		mod(gl_LaunchIDEXT.x/TILE_W + gl_LaunchIDEXT.y/TILE_H, 2));
}
