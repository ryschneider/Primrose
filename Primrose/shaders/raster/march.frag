#version 450

#define DEBUG
#include "../constants.glsl"
#include "../structs.glsl"
#include "../sdf.glsl"

layout(location = 0) in vec2 screenXY;
layout(location = 0) out vec4 fragColor;

// uniforms
layout(binding = 0, set = 0) uniform Block {
	MarchUniforms uniforms;
} uniformBlock;
#define u uniformBlock.uniforms

layout(binding = 1) uniform sampler2D texSampler;

layout(push_constant) uniform PushConstant {
	float time;
} push;

PointLight pointLights[] = {
	PointLight(vec3(0, 10, 0), vec3(1), 1)
};
const uint numPointLights = 1;
Material materials[] = { // color, emission, rough, spec, metal, ior, transmission
	Material(vec3(0.72, 0.45, 0.20), 0.05, 0.70, 0.08, 0.90, 1.70, 0.00), // copper
	Material(vec3(0.96, 0.99, 1.00), 0.05, 0.01, 0.42, 0.10, 1.45, 0.95), // glass
	Material(vec3(0.73, 0.95, 1.00), 0.05, 0.05, 2.15, 0.30, 2.40, 0.85), // diamond
};

// misc functions
float length2(vec3 v) {
	return dot(v, v); // returns x^2 + y^2 + z^2, ie length(v)^2
}

vec4 rand = texture(texSampler, (screenXY + 1) * 0.5);

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
	) - hit.d);
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
			float lastIor = bounce.insideMat == NO_MAT ? 1.0 : materials[bounce.insideMat].ior;
			float nextIor = bounce.insideMat == NO_MAT ? m.ior : 1.0;
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
	const vec3 forward = u.camDir.xyz;
	const vec3 right = normalize(cross(u.camUp.xyz, forward));
	const vec3 up = cross(forward, right);

	vec3 focalPos = u.camPos.xyz - u.focalLength*forward;

	vec3 fragPos = u.camPos.xyz
		+ (screenXY.x*right + screenXY.y*u.screenHeight*up) * u.invZoom;
	
	Ray ray = Ray(fragPos, normalize(fragPos - focalPos));
	
#ifdef DEBUG
	bool tileType = (mod(screenXY.x, TILE_SIZE) < TILE_SIZE/2.f) ^^ (mod(screenXY.y, TILE_SIZE) < TILE_SIZE/2.f);
	vec3 color = tileType ? vec3(0.f, 0.01f, 0.f) : vec3(0.01f, 0.f, 0.01f);
#else
	vec3 color = BG_COLOR;
#endif

	Bounce bounce = Bounce(ray, NO_MAT, 1);
	for (int i = 0; i < numBounces+1; ++i) {
		Hit hit = march(bounce.ray);

		if (hit.mat != NO_MAT) {
			hit = dither(hit, bounce.ray);
			renderMaterial(hit, bounce, color);
		}

		bounce = bounces[i];
	}

	fragColor = vec4(color, 1.f);
}
