#version 450

layout(location = 0) in vec2 screenXY;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform GlobalUniforms {
	float time;
	float screenHeight;
	vec3 camPos;
	vec3 camDir;
} u;

#define DEBUG

// should be uniforms
const float fov = radians(45.f);
const float focalLength = 1 / tan(fov / 2.f);

// constants
const vec3 BG_COLOR = vec3(0.01f, 0.01f, 0.01f);
const float MAX_DIST = 200.f;
const float HIT_MARGIN = 0.05f;
const float NORMAL_EPS = 0.0005f; // small epsilon for normal calculations

const int MAX_MARCHES = 200;
const int MAX_REFLECTIONS = 4;
const float REFL = 0.3f;

const float PI = 3.14159265358979323846264f;

#ifdef DEBUG
const float TILE_SIZE = 0.05f; // width/size of tiles
#endif

// misc structs
struct Ray {
	vec3 pos;
	vec3 dir;
};
struct Hit {
	vec3 pos; // position of hit
	float t; // ray t-value
	float d; // distance from surface
	bool hit; // if hit surface
};

// misc functions
float length2(vec3 v) {
	return dot(v, v); // returns x^2 + y^2 + z^2, ie length(v)^2
}

// primitives
mat4 rotationMatrix(float angle, vec3 axis) {
	// https://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle
	return mat4(
		vec4( // first column
			axis.x*axis.x*(1 - cos(angle)) + cos(angle),
			axis.y*axis.x*(1 - cos(angle)) + axis.z*sin(angle),
			axis.z*axis.x*(1 - cos(angle)) - axis.y*sin(angle),
			0),
		vec4( // second column
			axis.x*axis.y*(1 - cos(angle)) - axis.z*sin(angle),
			axis.y*axis.y*(1 - cos(angle)) + cos(angle),
			axis.z*axis.y*(1 - cos(angle)) + axis.x*sin(angle),
			0),
		vec4( // third column
			axis.x*axis.z*(1 - cos(angle)) + axis.y*sin(angle),
			axis.y*axis.z*(1 - cos(angle)) - axis.x*sin(angle),
			axis.z*axis.z*(1 - cos(angle)) + cos(angle),
			0),
		vec4(0, 0, 0, 1) // fourth column
	);
}
mat4 matrixFromRTS(mat4 rotation, vec3 translation, vec3 scale) {
	mat4 scaleMat = mat4(
		vec4(scale.x, 0.f, 0.f, 0.f), // first column
		vec4(0.f, scale.y, 0.f, 0.f), // second column
		vec4(0.f, 0.f, scale.z, 0.f), // third column
		vec4(0.f, 0.f, 0.f, 1.f) // fourth column
	);
	mat4 translateMat = mat4(
		vec4(1.f, 0.f, 0.f, 0.f), // first column
		vec4(0.f, 1.f, 0.f, 0.f), // second column
		vec4(0.f, 0.f, 1.f, 0.f), // third column
		vec4(translation.x, translation.y, translation.z, 1.f) // fourth column
	);

	return translateMat * rotation * scaleMat;
}
mat4 IDENTITY_MAT4 = mat4(
	vec4(1.f, 0.f, 0.f, 0.f),
	vec4(0.f, 1.f, 0.f, 0.f),
	vec4(0.f, 0.f, 1.f, 0.f),
	vec4(0.f, 0.f, 0.f, 1.f));

float smoothUnion(float d1, float d2, float k) {
	// https://iquilezles.org/articles/distfunctions/
	float h = clamp( 0.5f + 0.5f*(d2-d1)/k, 0.f, 1.f);
    return mix(d2, d1, h ) - k*h*(1.f-h);
}

// equals 0 at surface
float implicitSphere(vec3 p) {
	float x=p.x, y=p.y, z=p.z;
	return x*x + y*y + z*z - 9;
}
float implicitGenus2(vec3 p) {
	if (length2(p) > 9) return 100.f; // bounding sphere

	float x=p.x, y=p.y, z=p.z;
	float x2=x*x, y2=y*y, z2=z*z;
	float u = (x2+y2);
	return 2*y*(y2-3*x2)*(1-z2) + u*u - (9*z2-1)*(1-z2);
}
float implicitWineglass(vec3 p) {
	float x=p.x, y=p.y, z=p.z;
	float u = log(z+3.2);
	return x*x + y*y - u*u - 0.02;
}
float implicitKlein(vec3 p) {
	float x=p.x, y=p.y, z=p.z;
	float x2=x*x, y2=y*y, z2=z*z;
	float u = x2 + y2 + z2 - 1;
	float v = u - 2*y;
	return (u+2*y)*(v*v-8*z2) + 16*x*z*v;
}
// float implicitPlane(vec3 p) {
// 	float x=p.x, y=p.y, z=p.z;
// }

// #define WINDOWS
// #define VOLUME

Hit march(Ray ray) {
	Hit hit = Hit(
		ray.pos,
		0.f,
		0.f,
		false
	);

#ifdef WINDOWS
	// windows
	const float STEP = 0.01f;
	const float MARGIN = 0.1f;
	const float DIST = 50.f;
#else
	// mac
	const float STEP = 0.05f;
	const float MARGIN = 0.8f;
	const float DIST = 30.f;
#endif

	const int MARCHES = int(DIST / STEP);

	for (int m = 0; m < MARCHES; ++m) {
		hit.d = implicitKlein(hit.pos);
		
#ifdef VOLUME
		if (hit.d <= 0) {
#else
		if (abs(hit.d) <= MARGIN) {
#endif
			hit.hit = true;
			break;
		};
		if (hit.t >= MAX_DIST) {
			break;
		};

		hit.t += STEP;
		hit.pos += ray.dir*STEP;
	}

	return hit;
}

// main
void main() {
	const vec3 forward = u.camDir;
	const vec3 right = cross(forward, vec3(0.f, 1.f, 0.f));
	const vec3 up = cross(forward, right);

	vec3 focalPos = u.camPos - focalLength*forward;

	vec3 fragPos = u.camPos
		+ screenXY.x * right
		+ screenXY.y*u.screenHeight * up;
	
	Ray ray = Ray(fragPos, normalize(fragPos - focalPos));

#ifdef DEBUG
	bool tileType = (mod(screenXY.x, TILE_SIZE) < TILE_SIZE/2.f) ^^ (mod(screenXY.y, TILE_SIZE) < TILE_SIZE/2.f);
	vec3 color = tileType ? vec3(0.f, 0.1f, 0.f) : vec3(0.1f, 0.f, 0.1f);
#else
	vec3 color = BG_COLOR;
#endif
	
	Hit hit = march(ray);
	if (hit.hit) color = vec3((sin(hit.pos)+1.f)/2.f);

	// color = vec3(hit.t);
	
	fragColor = vec4(color, 1.f);
}
