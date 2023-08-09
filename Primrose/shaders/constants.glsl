#ifndef CONSTANTS_GLSL
#define CONSTANTS_GLSL

// identities
const uint PRIM_P1 = 101;
const uint PRIM_P2 = 102;
const uint PRIM_P3 = 103;
const uint PRIM_P4 = 104;
const uint PRIM_P5 = 105;
const uint PRIM_P6 = 106;
const uint PRIM_P7 = 107;
const uint PRIM_P8 = 108;
const uint PRIM_P9 = 109;
const uint PRIM_SPHERE = 201; // radius=1
const uint PRIM_BOX = 202; // sidelength=1
const uint PRIM_TORUS = 203; // params: ring radius (major radius=1)
const uint PRIM_LINE = 204; // params: half height (radius=1)
const uint PRIM_CYLINDER = 205; // radius=1

const uint OP_UNION = 901; // i(op) union j(op)
const uint OP_INTERSECTION = 902; // i(op) union j(op)
const uint OP_DIFFERENCE = 903; // i(op) - j(op)
const uint OP_IDENTITY = 904; // i(prim) identity
const uint OP_TRANSFORM = 905; // fragment position transformed by i(matrix)
const uint OP_RENDER = 906; // draw i(op) to screen

// constants
const vec3 BG_COLOR = vec3(0.01f, 0.01f, 0.01f);
const float MIN_DIST = 0.1f;
const float MAX_DIST = 10000.f;
const float HIT_MARGIN = 0.001f;
const float NORMAL_EPS = 0.0001f; // small epsilon for normal calculations
const uint NO_MAT = -1;

const int MAX_MARCHES = 100;
const uint MAX_BOUNCES = 5;

const float PI = 3.14159265358979323846264f;

#ifdef DEBUG
const float TILE_SIZE = 0.05f; // width/size of tiles
#endif

#endif
