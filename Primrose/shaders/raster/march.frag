#version 450

layout(location = 0) in vec2 screenXY;
layout(location = 0) out vec4 fragColor;

#define DEBUG



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

// uniforms
layout(binding = 0) uniform MarchUniforms {
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
PointLight pointLights[] = {
	PointLight(vec3(0, 10, 0), vec3(1), 1)
};
const uint numPointLights = 1;
Material materials[] = {
	//              base color, emission, rough, spec, metal, ior, transmission
	Material(vec3(0.72, 0.45, 0.20), 0.05, 0.70, 0.08, 0.90, 1.70, 0.00), // copper
	Material(vec3(0.96, 0.99, 1.00), 0.05, 0.01, 0.42, 0.10, 1.45, 0.95), // glass
	Material(vec3(0.73, 0.95, 1.00), 0.05, 0.05, 2.15, 0.30, 2.40, 0.85), // diamond
};

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstant {
	float time;
} push;

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



// misc functions
float length2(vec3 v) {
	return dot(v, v); // returns x^2 + y^2 + z^2, ie length(v)^2
}

vec4 rand = texture(texSampler, (screenXY + 1) * 0.5);

float smin(float d1, float d2, float k) {
	// https://iquilezles.org/articles/distfunctions/
	float h = clamp( 0.5f + 0.5f*(d2-d1)/k, 0.f, 1.f);
    return mix(d2, d1, h ) - k*h*(1.f-h);
}



// sdf functions
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

float lineSDF(vec3 p, float height) {
	p.y -= clamp(p.y, -height, height);
	return length(p) - 1;
}

float cylinderSDF(vec3 p) {
	return length(p.xz) - 1; // distance from the centre of xz plane
}

float prim1SDF(vec3 pos, float a) {
	// float t = p.time*0.7f - rand(a)*20.f;
//	float t = p.time*0.7f - a;
	float t = 0;

	float size = 0.2f * (4.f
		+ (sin(1.f*t)+1.f)
		* (sin(4.f*t)+1.f)
		* (sin(6.f*t)+1.f));
	float height = 0.5f;

//	float h = noise(normalize(pos)*size + t);
	float h = length(sin(normalize(pos)*size + t));
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
		case PRIM_LINE: return lineSDF(p, prim.a);
		case PRIM_CYLINDER: return cylinderSDF(p);
		case PRIM_P1: return prim1SDF(p, prim.a);
		case PRIM_P2: return prim2SDF(p, prim.a);
		case PRIM_P3: return prim3SDF(p);
		case PRIM_P4: return prim4SDF(p);
	}
}



// march algorithms
float mapMat(vec3 p, out uint mat) { // scene sdf
	float d = MAX_DIST;
	mat = NO_MAT;
	float dBuffer[100];
	uint matBuffer[100];

	vec4 pos = vec4(p, 1.f);
	float smallScale = 1.f;

	for (int i = 0; i < u.numOperations; ++i) {
		Operation op = u.operations[i];

		if (op.type == OP_TRANSFORM) {
			pos = u.transformations[op.i].invMatrix * vec4(p, 1.f);
			smallScale = u.transformations[op.i].smallScale;

		} else if (op.type == OP_IDENTITY) {
			Primitive prim = u.primitives[op.i];
			matBuffer[i] = op.j;
			dBuffer[i] = primSDF(pos.xyz, prim) * smallScale;

		} else if (op.type == OP_UNION) {
			float d1 = dBuffer[op.i];
			float d2 = dBuffer[op.j];
			matBuffer[i] = d1 < d2 ? matBuffer[op.i] : matBuffer[op.j];
			dBuffer[i] = min(d1, d2);

		} else if (op.type == OP_INTERSECTION) {
			float d1 = dBuffer[op.i];
			float d2 = dBuffer[op.j];
			matBuffer[i] = d1 > d2 ? matBuffer[op.i] : matBuffer[op.j];
			dBuffer[i] = max(d1, d2);

		} else if (op.type == OP_DIFFERENCE) {
			float d1 = dBuffer[op.i];
			float d2 = dBuffer[op.j];
			matBuffer[i] = d1 > -d2 ? matBuffer[op.i] : matBuffer[op.j];
			dBuffer[i] = max(d1, -d2);

		} else if (op.type == OP_RENDER) {
			mat = d < dBuffer[op.i] ? mat : matBuffer[op.i];
			d = min(d, dBuffer[op.i]);
		}
	}

	return d;
}
float map(vec3 p) {
	uint _;
	return mapMat(p, _);
}

vec3 mapNormal(Hit hit) {
	return normalize(vec3(
		map(hit.pos.xyz + vec3(NORMAL_EPS, 0.f, 0.f)),
		map(hit.pos.xyz + vec3(0.f, NORMAL_EPS, 0.f)),
		map(hit.pos.xyz + vec3(0.f, 0.f, NORMAL_EPS))
	) - hit.d);;
}

Hit march(Ray ray) {
	float d;
	float t = 0.f;
	vec3 pos = ray.pos;
	uint mat = NO_MAT;

	for (int m = 0; m < MAX_MARCHES; ++m) {
		d = abs(mapMat(pos, mat));
		
		if (d <= HIT_MARGIN && t >= MIN_DIST) break;
		if (t >= MAX_DIST) {
			mat = NO_MAT;
			break;
		}

		t += d;
		pos += ray.dir * d;
	}

	return Hit(pos, t, d, mat);
}

Hit dither(Hit hit, Ray ray) {
//	float b = rand(screenXY) * HIT_MARGIN + hit.d; // b in range (d - HIT_MARGIN, d)
	float b = rand.r * HIT_MARGIN + hit.d; // b in range (d - HIT_MARGIN, d)
	hit.pos += ray.dir * b;
	hit.d = map(hit.pos);
	return hit;
}



// material rendering
struct Bounce {
	Ray ray;
	uint insideMat;
	float strength;
};
Bounce bounces[MAX_BOUNCES+1];
uint numBounces = 0;

void renderMaterial(Hit hit, Bounce bounce, inout vec3 color) {
	Material m = materials[hit.mat];

	vec3 newColor = vec3(0);

	// reverse normal if inside material
	vec3 normal = bounce.insideMat == NO_MAT ? mapNormal(hit) : -mapNormal(hit);

	vec3 reflDir = reflect(bounce.ray.dir, normal);

	for (int i = 0; i < numPointLights; ++i) {
		PointLight light = pointLights[i];

		vec3 lightDir = normalize(light.pos - hit.pos);

		float diff = max(dot(normal, lightDir), 0.0);
		float spec = max(dot(reflDir, lightDir), 0.0);

		newColor += m.baseColor * (diff*m.roughness + spec*m.specular + m.emission) * light.intensity;
	}

	color = mix(color, newColor, bounce.strength);

	if (numBounces < MAX_BOUNCES) {
		float refl = m.metallic;
		if (m.transmission > 0) {
//			float ior;
//			if (bounce.insideMat == NO_MAT) ior = m.ior; // entering material
////			else if (bounce.insideMat != hit.mat) m.ior / materials[bounce.insideMat].ior; // switching material
//			else ior = 1.0 / materials[bounce.insideMat].ior; // exiting material

			float lastIor = bounce.insideMat == NO_MAT ? 1.0 : materials[bounce.insideMat].ior;
			float nextIor = bounce.insideMat == NO_MAT ? m.ior : 1.0;
//			float ior = nextIor / lastIor;
			float ior = lastIor / nextIor;

			vec3 refrDir = refract(bounce.ray.dir, normal, ior);

			if (length(refrDir) != 0) {
				bounces[numBounces].ray = Ray(hit.pos + refrDir*HIT_MARGIN, refrDir);
				bounces[numBounces].strength = bounce.strength * m.transmission;
//				bounces[numBounces].insideMat = bounce.insideMat != hit.mat ? hit.mat : NO_MAT;
				bounces[numBounces].insideMat = bounce.insideMat == NO_MAT ? hit.mat : NO_MAT;
				numBounces = min(numBounces+1, MAX_BOUNCES);
			} else {
				// internal reflection
				refl += m.transmission;
			}
		}

		if (refl > 0) {
			bounces[numBounces].ray = Ray(hit.pos + reflDir*HIT_MARGIN, reflDir);
			bounces[numBounces].strength = bounce.strength * m.metallic;
			bounces[numBounces].insideMat = bounce.insideMat;
			numBounces = min(numBounces+1, MAX_BOUNCES);
		}
	}
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
	vec3 color = tileType ? vec3(0.f, 0.01f, 0.f) : vec3(0.01f, 0.f, 0.01f);
#else
	vec3 color = BG_COLOR;
#endif
	
//	float reflCo = 1.f;
//	for (int r = 0; r < MAX_BOUNCES; ++r) {
//		Hit hit = march(ray);
//
//		if (hit.t <= MAX_DIST) {
//			dither(hit, ray);
//
//			renderMaterial(hit, ray, color);
//
////			vec3 normal = mapNormal(hit);
////			vec3 hitColor = abs(normal);
////			color = mix(color, hitColor, reflCo);
////
////			if (r != MAX_BOUNCES - 1) { // if not last reflection
////				ray.dir = reflect(ray.dir, normal);
////				ray.pos = hit.pos + ray.dir*HIT_MARGIN; // move 1 unit from surface to avoid collision
////
////				reflCo *= REFL;
////			}
//		} else {
//			break;
//		}
//	}

//	Bounce bounce = Bounce(ray, NO_MAT, 1);
//	for (int i = 0; i < numBounces+1; ++i) {
//		Hit hit = march(bounce.ray);
//
//		if (hit.mat != NO_MAT) {
//			hit = dither(hit, bounce.ray);
//			renderMaterial(hit, bounce, color);
//		}
//
//		bounce = bounces[i];
//	}

	fragColor = vec4(color, 1.f);

//	fragColor = vec4(vec3(march(ray).t), 1.f);
//	if (march(ray).mat != NO_MAT) {
//		fragColor = vec4(-ray.dir, 1.f);
//	} else {
//		fragColor = vec4(color, 1.f);
//	}
}
