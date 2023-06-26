
uint MAX_NUM_INSTRUCTIONS = 1000;
uint OP_UNION;
uint OP_INTERSECTION;
uint OP_VOLUME_INTERSECTION; // remove all points outside volume
uint OP_VOLUME_DIFFERENCE; // remove all points inside volume
struct Instruction {
	uint op;
	uint index;
};
Instruction instructions[MAX_NUM_INSTRUCTIONS];

uint MAX_NUM_PRIMITIVES = 200;
uint PRIM_SPHERE;
uint PRIM_CUBE;
struct Primitive {
	uint type;
	float a;
	float b;
	float c;
	float d;
	float e;
}
Primitive primitives[MAX_NUM_PRIMITIVES];


