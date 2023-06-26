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

const uint MAX_INTERSECTION_HITS = 2;
const float NO_INTERSECTION[MAX_INTERSECTION_HITS] = float[](NO_HIT, NO_HIT);

const uint MAX_GLOBAL_PRIM_HITS = 1000;

const vec3 BG_COLOR = vec3(0.01f, 0.01f, 0.01f);

// misc structs
struct Ray {
	vec3 pos;
	vec3 dir;
};

// primitive system
const uint PRIM_SPHERE = 1;
const uint PRIM_CUBE = 2;
struct Primitive {
	uint type; // PRIM_ prefix
	vec3 pos;
	vec4 rot;
	float a;
	float b;
	float c;
};
Primitive Sphere(vec3 pos, float r) {
	// uses position and radius
	return Primitive(PRIM_SPHERE, pos, vec4(0.f), r, 0.f, 0.f);
}
Primitive Cube(vec3 pos, vec4 rot, float s) {
	// uses position, rotation, and side length
	return Primitive(PRIM_CUBE, pos, rot, s, 0.f, 0.f);
}
Primitive primitives[] = {
	// // wall 50 units away
	// Sphere(vec3(0.f, 0.0f, 1050.f), 1000.f),

	Sphere(vec3(-2.f, -1.f, -2.f), 2.f),
	Sphere(vec3(2.f, 1.f, 2.f), 2.f),
	
	Sphere(vec3(-2.8f, -1.f, -2.8f), 1.f),
	Sphere(vec3(2.8f, 1.f, 2.8f), 1.f),
};

// csg system
const uint TYPE_CSG_CSG = 1; // csgs[a], csgs[b]
const uint TYPE_CSG_PRIM = 2; // csgs[a], primitives[b]
const uint TYPE_PRIM_CSG = 3; // primitives[a], csgs[b]
const uint TYPE_PRIM_PRIM = 4; // primitives[a], primitives[b]
const uint TYPE_PRIM = 5; // no operation, primitives[a]
const uint OP_UNION = 1; // csg = a || b
const uint OP_INTERSECTION = 2; // csg = a && b
const uint OP_DIFFERENCE = 3; // csg = a - b
const uint OP_NONE = 4; // csg = a - b

struct Csg {
	uint type; // TYPE_ prefix
	uint operation; // OP_ prefix
	uint a; // refers to the index of the object in
	uint b; // either csgs[] or primitives[] depending on type
};
Csg PrimitiveCsg(uint primIndex) {
	return Csg(TYPE_PRIM, OP_NONE, primIndex, NO_PRIM);
}

Csg csgs[] = {
	// PrimitiveCsg(0),
	// PrimitiveCsg(2)
	Csg(TYPE_PRIM_PRIM, OP_DIFFERENCE, 0, 2),
	Csg(TYPE_PRIM_PRIM, OP_DIFFERENCE, 1, 3)
};
uint renderCsgs[] = {
	0, 1
};

struct PrimHit {
	float t; // t-value
	uint prim; // prim index
};
const PrimHit NO_PRIM_HIT = PrimHit(NO_HIT, NO_PRIM);

// PrimHit globalPrimHits[MAX_GLOBAL_PRIM_HITS];
// uint numGlobalPrimHits = 0;

// misc functions

float length2(vec3 v) {
	return dot(v, v); // returns x^2 + y^2 + z^2, ie length(v)^2
}

vec3 hsv2rgb(float h, float s, float v)
{ // https://gist.github.com/983/e170a24ae8eba2cd174f
	vec3 c = vec3(h, s, v);
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// primitive functions
float[MAX_INTERSECTION_HITS] sphereIntersection(Ray ray, uint sphereIndex) {
	// https://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection

	// sphere parameters
	const Primitive sphere = primitives[sphereIndex];
	const float r = sphere.a;

	vec3 rayToCentre = sphere.pos - ray.pos;
	float base = dot(rayToCentre, ray.dir);
	// filters out objects behind the camera

	float discr = base*base - length2(rayToCentre) + r*r;
	if (discr < 0.f) return NO_INTERSECTION; // no real solutions
	float delta = sqrt(discr);

	// didn't bother checking for delta=0, could make performance better/worse idk
	if (base - delta >= 0) {
		return float[](base-delta, base+delta);
	} else if (base + delta >= 0) {
		return float[](base+delta, NO_HIT);
	} else {
		return NO_INTERSECTION;
	}
}
float[MAX_INTERSECTION_HITS] cubeIntersection(Ray ray, uint cubeIndex) {
	return NO_INTERSECTION;
}
float[MAX_INTERSECTION_HITS] primIntersection(Ray ray, uint primIndex) {
	const Primitive prim = primitives[primIndex];

	if (prim.type == PRIM_SPHERE) {
		return sphereIntersection(ray, primIndex);
	} else if (prim.type == PRIM_CUBE) {
		return cubeIntersection(ray, primIndex);
	}
	#ifdef DEBUG
	else {
		// if the prim type is invalid it should make every frag hit, should be enough to notice & fix
		return float[](0.f, 0.f);
	}
	#endif
}

bool primContainsPoint(vec3 p, uint primIndex) {
	if (primitives[primIndex].type == PRIM_SPHERE) {
		// sphere constants
		const Primitive sphere = primitives[primIndex];
		const float r = sphere.a;

		vec3 distVec = sphere.pos - p;
		float dist2 = length2(distVec); // squared dist between p and sphere

		if (dist2 <= r*r) {
			// point is in sphere if distance is smaller than r
			// ie distance^2 is smaller than r^2
			return true;
		} else {
			return false;
		}
	} else if (primitives[primIndex].type == PRIM_CUBE) {
		return true;
	}
	#ifdef DEBUG
	else {
		// if the prim type is invalid it should make every frag hit, should be enough to notice & fix
		return true;
	}
	#endif
}

vec3 primNormal(vec3 hit, Primitive prim) {
	if (prim.type == PRIM_SPHERE) {
		return normalize(hit - prim.pos);
	} else if (prim.type == PRIM_CUBE) {
		return vec3(0.f, 1.f, 0.f);
	}
	#ifdef DEBUG
	else {
		return vec3(0.f, 0.f, 0.f);
	}
	#endif
}
vec3 primColor(vec3 hit, Primitive prim) {
	if (prim.type == PRIM_SPHERE) {
		Primitive sphere = prim; // sphere vals
		float r = sphere.a;

		vec3 relPos = hit - prim.pos;
		float theta = atan(relPos.z / relPos.x) + 3.1415f/2.f;
		if (relPos.x < 0) { // increase range to 2pi
			theta += 3.1415;
		}
		float hue = theta / (2*3.1415);

		float sat = (relPos.y+r) / (2*r);

		return hsv2rgb(hue, sat, 1.f);
		// return vec3(hue);
	} else if (prim.type == PRIM_CUBE) {
		return vec3(0.f, 1.f, 0.f);
	}
	#ifdef DEBUG
	else {
		return vec3(0.f, 0.f, 0.f);
	}
	#endif
}

// csg functions
PrimHit renderUnionPP(Ray ray, uint primIndexA, uint primIndexB) {
	float aHits[] = primIntersection(ray, primIndexA);
	float bHits[] = primIntersection(ray, primIndexB);

	PrimHit hit = NO_PRIM_HIT; // best hit found

	// check a and b hits
	for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
		float t = aHits[i];
		if (t < hit.t) {
			hit.t = t;
			hit.prim = primIndexA; // hit the surface of A, inside B
		}
	}
	for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
		float t = bHits[i];
		if (t < hit.t) {
			hit.t = t;
			hit.prim = primIndexB; // hit the surface of A, inside B
		}
	}

	return hit;
}

PrimHit renderIntersectionPP(Ray ray, uint primIndexA, uint primIndexB) {
	float aHits[] = primIntersection(ray, primIndexA);
	float bHits[] = primIntersection(ray, primIndexB);

	PrimHit hit = NO_PRIM_HIT;

	// check for the A surface intersections inside B
	for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
		float t = aHits[i];
		vec3 point = ray.pos + t*ray.dir;
		if (primContainsPoint(point, primIndexB)) {
			if (t < hit.t) {
				hit.t = t;
				hit.prim = primIndexA; // hit the surface of A, inside B
			}
		}
	}

	// check for the B surface intersections inside A
	for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
		float t = bHits[i];
		vec3 point = ray.pos + t*ray.dir;
		if (primContainsPoint(point, primIndexA)) {
			if (t < hit.t) {
				hit.t = t;
				hit.prim = primIndexB; // hit the surface of A, inside B
			}
		}
	}

	return hit;
}

PrimHit renderDifferencePP(Ray ray, uint primIndexA, uint primIndexB) {
	float aHits[] = primIntersection(ray, primIndexA);
	float bHits[] = primIntersection(ray, primIndexB);

	PrimHit hit = NO_PRIM_HIT;

	// get all A hits not inside B
	for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
		float t = aHits[i];
		vec3 point = ray.pos + t*ray.dir;
		if (!primContainsPoint(point, primIndexB)) {
			if (t < hit.t) {
				hit.t = t;
				hit.prim = primIndexA; // hit the surface of A, inside B
			}
		}
	}

	// get all B bits inside A
	for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
		float t = bHits[i];
		vec3 point = ray.pos + t*ray.dir;
		if (primContainsPoint(point, primIndexA)) {
			if (t < hit.t) {
				hit.t = t;
				hit.prim = primIndexB; // hit the surface of A, inside B
			}
		}
	}

	return hit;
}

PrimHit renderIdentityP(Ray ray, uint primIndex) {
	float hits[] = primIntersection(ray, primIndex);

	PrimHit hit = NO_PRIM_HIT;
	for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
		float t = hits[i];
		if (t < hit.t) {
			hit.t = t;
			hit.prim = primIndex;
		}
	}

	return hit;
}

PrimHit renderUnionPC(Ray ray, uint primIndex, uint csgIndex) {
	
	uint currentCsgIndex = csgIndex;
	PrimHit allPrimHits[100];
	uint numPrimHits = 0;

	while (currentCsgIndex != NO_CSG) {
		Csg currentCsg = csgs[currentCsgIndex];

		if (currentCsg.type == TYPE_PRIM_PRIM) {

			const uint primIndexA = currentCsg.a;
			const uint primIndexB = currentCsg.b;

			float hitsA[] = primIntersection(ray, primIndexA);
			float hitsB[] = primIntersection(ray, primIndexB);

			for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
				if (hitsA[i] == NO_HIT) break;
				allPrimHits[numPrimHits++] = PrimHit(hitsA[i], primIndexA);
			}
			for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
				if (hitsB[i] == NO_HIT) break;
				allPrimHits[numPrimHits++] = PrimHit(hitsB[i], primIndexB);
			}

		} else if (currentCsg.type == TYPE_PRIM_CSG) {

			const uint primIndex = currentCsg.a;
			const uint csgIndex = currentCsg.b;

			float hits[] = primIntersection(ray, primIndex);

			for (uint i = 0; i < MAX_INTERSECTION_HITS; ++i) {
				if (hits[i] == NO_HIT) break;
				allPrimHits[numPrimHits++] = PrimHit(hits[i], primIndex);
			}

			currentCsgIndex = csgIndex;

		}
	}

	return NO_PRIM_HIT;

}

PrimHit renderIntersectionPC(Ray ray, uint primIndex, uint csgIndex) {
	return NO_PRIM_HIT;
}

PrimHit renderDifferencePC(Ray ray, uint primIndex, uint csgIndex) {
	return NO_PRIM_HIT;
}

PrimHit renderDifferenceCP(Ray ray, uint csgIndex, uint primIndex) {
	return NO_PRIM_HIT;
}

PrimHit render(Ray ray, Csg csg) {
	if (csg.type == TYPE_PRIM_CSG) {

		uint primIndex = csg.a;
		uint csgIndex = csg.b;

		if (csg.operation == OP_UNION) {
			return renderUnionPC(ray, primIndex, csgIndex);
		} else if (csg.operation == OP_INTERSECTION) {
			return renderIntersectionPC(ray, primIndex, csgIndex);
		} else if (csg.operation == OP_DIFFERENCE) {
			return renderDifferencePC(ray, primIndex, csgIndex);
		}
		
	}
	
	else if (csg.type == TYPE_CSG_PRIM) {

		uint csgIndex = csg.a;
		uint primIndex = csg.b;

		if (csg.operation == OP_UNION) { // prim-csg is equivalent to csg-prim for union and intersection
			return renderUnionPC(ray, primIndex, csgIndex);
		} else if (csg.operation == OP_INTERSECTION) {
			return renderIntersectionPC(ray, primIndex, csgIndex);
		} else if (csg.operation == OP_DIFFERENCE) { // difference is not commutative so it has its own func
			return renderDifferenceCP(ray, csgIndex, primIndex);
		}

	}

	else if (csg.type == TYPE_PRIM_PRIM) {

		uint primIndexA = csg.a;
		uint primIndexB = csg.b;

		if (csg.operation == OP_UNION) {
			return renderUnionPP(ray, primIndexA, primIndexB);
		} else if (csg.operation == OP_INTERSECTION) {
			return renderIntersectionPP(ray, primIndexA, primIndexB);
		} else if (csg.operation == OP_DIFFERENCE) {
			return renderDifferencePP(ray, primIndexA, primIndexB);
		}

	} else if (csg.type == TYPE_PRIM) {

		uint primIndex = csg.a;

		if (csg.operation == OP_NONE) {
			return renderIdentityP(ray, primIndex);
		}

	}

	#ifdef DEBUG
	discard;
	#else
	return NO_PRIM_HIT;
	#endif
}

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
	
	PrimHit bestHit = NO_PRIM_HIT;
	for (int i = 0; i < renderCsgs.length(); ++i) {
		uint csgIndex = renderCsgs[i];
		Csg csg = csgs[csgIndex];

		PrimHit hit = render(ray, csg);

		if (hit.t < bestHit.t) {
			bestHit = hit;
		}
	}

	if (bestHit == NO_PRIM_HIT) {
		fragColor = vec4(BG_COLOR, 1.f);
	} else {
		vec3 hitPos = ray.pos + ray.dir * bestHit.t;
		vec3 color = primColor(hitPos, primitives[bestHit.prim]);
		fragColor = vec4(color, 1.f);
	}
}
