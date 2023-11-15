#ifndef STRUCTS_GLSL
#define STRUCTS_GLSL

// uniform structures
struct Operation {
	uint type; // OP_ prefix
	uint i; // index of first operand
	uint j; // index of second operand
};

struct Primitive {
	uint type; // PRIM:: prefix
	float a; // first parameter
	float b; // second parameter
	uint mat; // material id
};

struct Transformation {
	mat4 invMatrix;
	float smallScale;
};

struct Material {
	vec3 baseColor;

	float emission;
	float roughness;
	float specular;

	float metallic;

	float ior;
	float transmission;
};

struct PointLight {
	vec3 pos;
	vec3 color;
	float intensity;
};

struct ModelAttributes {
	mat4 invMatrix;
	float invScale;
	vec3 aabbMin;
	vec3 aabbMax;
};

struct MarchUniforms {
	vec3 camPos;
	vec3 camDir;
	vec3 camUp;

	float screenHeight;

	float focalLength;
	float invZoom;

	ModelAttributes attributes[100];
	uint geometryAttributeOffset[100];

//	// pre-computed
//	vec3 camPosRcp;

	uint numOperations;
	Operation operations[100];
	Primitive primitives[100];
	Transformation transformations[100];
};

// march structs
struct Ray {
	vec3 pos;
	vec3 dir;
};

struct Hit {
	vec3 pos; // position of hit
	float t; // ray t-value
	float d; // distance from surface
	uint mat; // material index
};

#endif
