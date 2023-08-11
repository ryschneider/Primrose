#version 460 core
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT Payload {
	vec3 color;
} payload;

void main() {
	payload.color = vec3(1, 0, 0);
}
