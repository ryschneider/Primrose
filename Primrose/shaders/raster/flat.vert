#version 450

layout(location = 0) out vec2 uv;

void main() {
	// if (gl_VertexIndex == 0) {
	// 	uv = vec2(-1.0f, 1.0f);
	// } else if (gl_VertexIndex == 1) {
	// 	uv = vec2(4.0f, 1.0f);
	// } else if (gl_VertexIndex == 2) {
	// 	uv = vec2(-1.0f, -4.0f);
	// }

	uv = vec2(gl_VertexIndex == 1 ? 4.f : -1.f, gl_VertexIndex == 2 ? -4.f : 1.f);
	
	gl_Position = vec4(uv.x, -uv.y, 0.0f, 1.0f);
}
