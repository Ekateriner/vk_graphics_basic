//#version 450
//#extension GL_ARB_separate_shader_objects : enable
//#extension GL_GOOGLE_include_directive : require
//
//// Author @patriciogv - 2015
//// http://patriciogonzalezvivo.com

#ifdef GL_ES
precision mediump float;
#endif

float random(float p) {
    p = fract(p * 0.011);
    p *= p + 7.5;
    p *= p + p;
    return fract(p);
}

// Based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise(in vec3 x) {
    const vec3 step = vec3(110, 241, 171);

    vec3 i = floor(x);
    vec3 f = fract(x);

    // For performance, compute the base input to a 1D hash from the integer part of the argument and the
    // incremental change to the 1D based on the 3D -> 1D wrapping
    float n = dot(i, step);

    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix( random(n + dot(step, vec3(0, 0, 0))), random(n + dot(step, vec3(1, 0, 0))), u.x),
                   mix( random(n + dot(step, vec3(0, 1, 0))), random(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
               mix(mix( random(n + dot(step, vec3(0, 0, 1))), random(n + dot(step, vec3(1, 0, 1))), u.x),
                   mix( random(n + dot(step, vec3(0, 1, 1))), random(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

#define NUM_OCTAVES 5

float fbm(in vec3 x) {
    float v = 0.0;
    float a = 0.5;
    vec3 shift = vec3(100.0);
    // Rotate to reduce axial bias
    mat2 rot = mat2(cos(0.5), sin(0.5),
                    -sin(0.5), cos(0.50));

    for (int i = 0; i < NUM_OCTAVES; ++i) {
        v += a * noise(x);
        x = x * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

float density(in vec3 pos, in float u_time) {
    // st += st * abs(sin(u_time*0.1)*3.0);
    vec3 q = vec3(0.);
    q.x = fbm(pos + 0.0 * u_time);
    //q.y = fbm(pos + vec3(0.0));
    q.z = fbm(pos + vec3(1.0, 0.0, 1.0));

    vec3 r = vec3(0.);
    r.x = fbm(pos + 1.0*q + vec3(1.7, 0, 9.2) + 0.15 * u_time);
    //r.y = fbm(pos + 1.0*q + 0.126 * u_time);
    r.z = fbm(pos + 1.0*q + vec3(8.3, 0, 2.8) + 0.126 * u_time);

    float f = fbm(pos+r);
    float dens = mix(0.666667, 0.498039, clamp((f*f),0.0,1.0));
    dens = mix(dens, 0.164706, clamp(length(q),0.0,1.0));
    dens = mix(dens, 1.0, clamp(length(r.x),0.0,1.0));

    //(f*f*f+.6*f*f+.5*f)
    return clamp((f*f*f+f*f+f) * dens, 0.0, 1.0);
}
