#ifndef SDF_GLSL
#define SDF_GLSL

#include "constants.glsl"

// misc functions
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
    return smin(dx, smin(dy, smin(dz,  smin(dxm, smin(dym, dzm, k), k), k), k), k);
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

#endif
