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
const float fov = radians(90);
const float focalLength = 1 / tan(fov / 2.f);

// constants
const float NO_HIT = 3.402823466e+38; // t-value, max float value
const uint NO_PRIM = 65535; // prim index, max int value (16 bit)
const uint NO_CSG = 65535; // csg index, max int value (16 bit)

const vec3 BG_COLOR = vec3(0.01f, 0.01f, 0.01f);

const uint MAX_NUM_INSTRUCTIONS = 1000;
const uint MAX_NUM_INSTRUCTION_SETS = 100;
const uint MAX_NUM_PRIMITIVES = 200;

// misc structs
struct Ray {
	vec3 pos;
	vec3 dir;
};



// primitives
const uint PRIM::SPHERE = 1;
const uint PRIM::CUBE = 2;
struct Primitive {
	uint type; // PRIM:: prefix
	vec3 pos;
	vec4 rot;
	float a;
	float b;
	float c;
};
// prim initialisers
Primitive Sphere(vec3 pos, float r) { // uses position and radius
	return Primitive(PRIM::SPHERE, pos, vec4(0.f), r, 0.f, 0.f); }
Primitive Cube(vec3 pos, vec4 rot, float s) { // uses position, rotation, and side length
	return Primitive(PRIM::CUBE, pos, rot, s, 0.f, 0.f); }
Primitive primitives[MAX_NUM_PRIMITIVES] = {
	Sphere(vec3(-1.f, 0.f, -1.f), 2.f),
	Sphere(vec3(1.f, 0.f, 1.f), 2.f)
};



// instructions
const uint OP::ADD_POINTS = 1;
const uint OP::VOLUME_INTERSECTION = 3; // remove all points outside volume
const uint OP::VOLUME_DIFFERENCE = 4; // remove all points inside volume
struct Instruction {
	uint op; // operation to perform
	uint index; // index used by operation
};
Instruction instructions[MAX_NUM_INSTRUCTIONS] = {
	Instruction(OP::ADD_POINTS, 0),
	Instruction(OP::VOLUME_INTERSECTION, 1)
};



// instruction sets
struct InstructionSet {
	uint startInstruction;
	uint numInstructions;
}
InstructionSet instructionSets[MAX_NUM_INSTRUCTION_SETS] = {
	InstructionSet(0, 2)
};




// main
void main() {
	const vec3 forward = u.camDir;
	const vec3 right = cross(forward, vec3(0.f, 1.f, 0.f));
	const vec3 up = cross(forward, right);

	vec3 focalPos = u.camPos - focalLength*forward;

	vec3 fragPos = u.camPos
		+ screenXY.x*right
		+ screenXY.y*up * u.screenHeight;
	
	Ray ray = {
		fragPos,
		normalize(fragPos - focalPos)
	};

	for (int si = 0; si < MAX_NUM_INSTRUCTION_SETS; ++i) {
		
	}
	
	// PrimHit bestHit = NO_PRIM::HIT;
	// for (int i = 0; i < renderCsgs.length(); ++i) {
	// 	uint csgIndex = renderCsgs[i];
	// 	Csg csg = csgs[csgIndex];

	// 	PrimHit hit = render(ray, csg);

	// 	if (hit.t < bestHit.t) {
	// 		bestHit = hit;
	// 	}
	// }

	if (bestHit == NO_PRIM::HIT) {
		fragColor = vec4(BG_COLOR, 1.f);
	} else {
		vec3 hitPos = ray.pos + ray.dir * bestHit.t;
		vec3 color = primColor(hitPos, primitives[bestHit.prim]);
		fragColor = vec4(color, 1.f);
	}
}
