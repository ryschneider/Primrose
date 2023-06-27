#version 450

layout(location = 0) in vec2 screenXY;
layout(location = 0) out vec4 fragColor;

#define DEBUG

const uint PRIM_1 = 101;
const uint PRIM_2 = 102;
const uint PRIM_3 = 103;
const uint PRIM_4 = 104;
const uint PRIM_5 = 105;
const uint PRIM_6 = 106;
const uint PRIM_7 = 107;
const uint PRIM_8 = 108;
const uint PRIM_9 = 109;
const uint PRIM_SPHERE = 201;
const uint PRIM_BOX = 202;
const uint PRIM_TORUS = 203; // params: minor radius
const uint PRIM_LINE = 204; // params: half height, radius
const uint PRIM_CYLINDER = 205;

const uint MAT_1 = 301;
const uint MAT_2 = 302;
const uint MAT_3 = 303;

const uint OP_UNION = 901; // i(op) union j(op)
const uint OP_INTERSECTION = 902; // i(op) union j(op)
const uint OP_DIFFERENCE = 903; // i(op) - j(op)
const uint OP_IDENTITY = 904; // i(op) identity
const uint OP_TRANSFORM = 905; // fragment position transformed by i(matrix)

struct Primitive {
	mat4 invTransform; // inverse of transformation matrix
	uint type; // PRIM_ prefix
	float smallScale; // smallest scalar
	float a; // first parameter
	float b; // second parameter
	uint mat; // material id
};

struct Operation {
	uint type; // OP_ prefix
	uint i; // index of first operand
	uint j; // index of second operand
	bool render; // whether to render operation
};

struct Transformation {
	mat4 invMatrix;
	float smallScale;
};

layout(set = 0, binding = 0) uniform GlobalUniforms {
	vec3 camPos;
	vec3 camDir;
	vec3 camUp;
	float screenHeight;

	float focalLength;
	float invZoom;

	uint numOperations;
	Operation operations[100];
	Primitive primitives[100];
	Transformation transformations[100];
} u;

layout(push_constant) uniform PushConstant {
	float time;
} p;

// constants
const vec3 BG_COLOR = vec3(0.01f, 0.01f, 0.01f);
const float MAX_DIST = 1000.f;
const float HIT_MARGIN = 0.05f;
const float NORMAL_EPS = 0.001f; // small epsilon for normal calculations

const int MAX_MARCHES = 100;
const int MAX_REFLECTIONS = 2;
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
};

// misc functions
float length2(vec3 v) {
	return dot(v, v); // returns x^2 + y^2 + z^2, ie length(v)^2
}

vec4 permute(vec4 x){return mod(((x*34.0)+1.0)*x, 289.0);}
vec4 taylorInvSqrt(vec4 r){return 1.79284291400159 - 0.85373472095314 * r;}
vec3 fade(vec3 t) {return t*t*t*(t*(t*6.0-15.0)+10.0);}
float noise(vec3 P){
  vec3 Pi0 = floor(P); // Integer part for indexing
  vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
  Pi0 = mod(Pi0, 289.0);
  Pi1 = mod(Pi1, 289.0);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute(permute(ix) + iy);
  vec4 ixy0 = permute(ixy + iz0);
  vec4 ixy1 = permute(ixy + iz1);

  vec4 gx0 = ixy0 / 7.0;
  vec4 gy0 = fract(floor(gx0) / 7.0) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 / 7.0;
  vec4 gy1 = fract(floor(gx1) / 7.0) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_xyz = fade(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x); 
  return 2.2 * n_xyz;
}
float rand(float n){return fract(sin(n) * 43758.5453123);}
float smin(float d1, float d2, float k) {
	// https://iquilezles.org/articles/distfunctions/
	float h = clamp( 0.5f + 0.5f*(d2-d1)/k, 0.f, 1.f);
    return mix(d2, d1, h ) - k*h*(1.f-h);
}

float sphereSDF(vec3 p) { // radius 1
	return length(p) - 1.f;
}

float cubeSDF(vec3 p) { // side length 1
	// https://www.youtube.com/watch?v=62-pRVZuS5c

	vec3 q = abs(p) - 1.f;
	return length(max(q, 0.f)) + min(max(q.x, max(q.y, q.z)), 0.f);

	// return length(max(abs(p) - 1, 0));
}

float torusSDF(vec3 p, float minorRadius) { // major radius 1
	// https://iquilezles.org/articles/distfunctions
	return length(vec2(length(p.xz) - 1.f, p.y)) - minorRadius;
}

float lineSDF(vec3 p, float height, float radius) {
	p.y -= clamp(p.y, -height, height);
	return length(p) - radius;
}

float cylinderSDF(vec3 p) {
	return length(p.xz) - 1; // distance from the centre of xz plane
}

float prim1SDF(vec3 pos, float a) {
	// float t = p.time*0.7f - rand(a)*20.f;
	float t = p.time*0.7f - a;

	float size = 0.2f * (4.f
		+ (sin(1.f*t)+1.f)
		* (sin(4.f*t)+1.f)
		* (sin(6.f*t)+1.f));
	float height = 0.5f;

	float h = noise(normalize(pos)*size + t);
	h *= h;

	return length(pos) - a*(1.f + h*height);
}

float prim2SDF(vec3 p, float radius) {
	// coordinate axes

	float x2 = p.x*p.x;
	float y2 = p.y*p.y;
	float z2 = p.z*p.z;
	
	float dx = sqrt(y2 + z2) - radius;
	float dy = sqrt(x2 + z2) - radius;
	float dz = sqrt(x2 + y2) - radius;
	
	float mr = radius*1.5f; // marker radius
	float mw = radius*0.2f; // marker width
	float md = radius*6.f; // marker distance from origin
	float mc = radius*0.1f; // marker corner radius
	
	// vec3 p_x = vec3(p.x - md, p.y, p.z);
	// vec2 d_x = abs(vec2(length(p_x.yz), p_x.x)) - vec2(mr, mw);
	// vec2 d_x = vec2(, );
	float dxm = min(max(length(p.yz)-mr,abs(p.x-md)-mw),0.f)+length(max(vec2(length(p.yz)-mr,abs(p.x-md)-mw),0.f))-mc;
	float dym = min(max(length(p.xz)-mr,abs(p.y-md)-mw),0.f)+length(max(vec2(length(p.xz)-mr,abs(p.y-md)-mw),0.f))-mc;
	float dzm = min(max(length(p.xy)-mr,abs(p.z-md)-mw),0.f)+length(max(vec2(length(p.xy)-mr,abs(p.z-md)-mw),0.f))-mc;
	// float dym = 1000.f;
	// float dzm = 1000.f;

	float k = radius*2.f;
	return smin(dx,
		smin(dy,
		smin(dz,
		smin(dxm,
		smin(dym,
		dzm, k), k), k), k), k);
	// return smin(dx, smin(dy, dz, k), k);
}

float prim3SDF(vec3 p) {
	return -prim2SDF(p, 40.f);
}

float prim4SDF(vec3 p) {
	float c = 10;
	vec3 q = mod(p + 0.5*c, vec3(c)) - vec3(0.5*c);
	return torusSDF(q, 0.3f);
}

float primSDF(vec3 p, Primitive prim) {
	switch (prim.type) {
		case PRIM_SPHERE: return sphereSDF(p);
		case PRIM_BOX: return cubeSDF(p);
		case PRIM_TORUS: return torusSDF(p, prim.a);
		case PRIM_LINE: return lineSDF(p, prim.a, prim.b);
		case PRIM_CYLINDER: return cylinderSDF(p);
		case PRIM_1: return prim1SDF(p, prim.a);
		case PRIM_2: return prim2SDF(p, prim.a);
		case PRIM_3: return prim3SDF(p);
		case PRIM_4: return prim4SDF(p);
	}
}

float map(vec3 p) { // scene sdf
	float d = MAX_DIST;
	float dists[100];

	vec4 pos = vec4(p, 1.f);
	float smallScale = 1.f;

	for (int i = 0; i < u.numOperations; ++i) {
		Operation op = u.operations[i];

		if (op.type == OP_TRANSFORM) {
			pos = u.transformations[op.i].invMatrix * vec4(p, 1.f);
			smallScale = u.transformations[op.i].smallScale;

		} else if (op.type == OP_IDENTITY) {
			Primitive prim = u.primitives[op.i];
			dists[i] = primSDF(pos.xyz, prim) * smallScale;

		} else if (op.type == OP_UNION) {
			float d1 = dists[op.i];
			float d2 = dists[op.j];
			dists[i] = min(d1, d2);

		} else if (op.type == OP_INTERSECTION) {
			float d1 = dists[op.i];
			float d2 = dists[op.j];
			dists[i] = max(d1, d2);

		} else if (op.type == OP_DIFFERENCE) {
			float d1 = dists[op.i];
			float d2 = dists[op.j];
			dists[i] = max(d1, -d2);
		}

		if (op.render) d = min(d, dists[i]);
	}

//	vec4 pos4 = vec4(p, 1.f);
//	for (int i = 0; i < u.numPrimitives; ++i) {
//		Primitive prim = u.primitives[i];
//
//		vec4 pos = prim.invTransform * pos4; // apply reverse transformation to correct sdf
//		float primD = primSDF(pos.xyz, prim);
//		primD *= prim.smallScale;
//
//		// primD -= 0.1f; // rounded corners
//		// d = smoothUnion(d, primD, 1.5f); // smoothing
//		d = min(d, primD);
//	}

	return d;
}

vec3 mapNormal(Hit hit) {
	return normalize(vec3(
		map(hit.pos.xyz + vec3(NORMAL_EPS, 0.f, 0.f)),
		map(hit.pos.xyz + vec3(0.f, NORMAL_EPS, 0.f)),
		map(hit.pos.xyz + vec3(0.f, 0.f, NORMAL_EPS))
	) - hit.d);;
}

// vec3 mapNormal( Hit hit ) // for function f(p)
// {
//     const float h = NORMAL_EPS; // replace by an appropriate value
//     const vec2 k = vec2(1,-1);
//     return normalize( k.xyy*map( hit.pos.xyz + k.xyy*h ) + 
//                       k.yyx*map( hit.pos.xyz + k.yyx*h ) + 
//                       k.yxy*map( hit.pos.xyz + k.yxy*h ) + 
//                       k.xxx*map( hit.pos.xyz + k.xxx*h ) );
// }

Hit march(Ray ray) {
	float d;
	float t = 0.f;
	vec3 pos = ray.pos;

	for (int m = 0; m < MAX_MARCHES; ++m) {
		d = map(pos);
		
		if (d <= HIT_MARGIN || t >= MAX_DIST) break;

		t += d;
		pos += ray.dir*d;
	}

	return Hit(pos, t, d);
}

// main
void main() {
	const vec3 forward = u.camDir;
	const vec3 right = normalize(cross(u.camUp, forward));
	const vec3 up = cross(forward, right);

	vec3 focalPos = u.camPos - u.focalLength*forward;

	vec3 fragPos = u.camPos
		+ (screenXY.x*right + screenXY.y*u.screenHeight*up) * u.invZoom;
	
	Ray ray = Ray(fragPos, normalize(fragPos - focalPos));
	
#ifdef DEBUG
	bool tileType = (mod(screenXY.x, TILE_SIZE) < TILE_SIZE/2.f) ^^ (mod(screenXY.y, TILE_SIZE) < TILE_SIZE/2.f);
	vec3 color = tileType ? vec3(0.f, 0.02f, 0.f) : vec3(0.02f, 0.f, 0.02f);
#else
	vec3 color = BG_COLOR;
#endif
	
	float reflCo = 1.f;
	for (int r = 0; r < MAX_REFLECTIONS; ++r) {
		Hit hit = march(ray);

		if (hit.d <= HIT_MARGIN) {
			vec3 normal = mapNormal(hit);

			vec3 hitColor = abs(normal);
			color = mix(color, hitColor, reflCo);

			if (r != MAX_REFLECTIONS-1) { // if not last reflection
				ray.dir = reflect(ray.dir, normal);
				ray.pos = hit.pos + ray.dir*HIT_MARGIN; // move 1 unit from surface to avoid collision

				reflCo *= REFL;
			}

		} else {
			break;
		}
	}

	fragColor = vec4(color, 1.f);
//	fragColor = vec4(.2f);
}
