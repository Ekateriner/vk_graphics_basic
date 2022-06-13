#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out float out_ssaoColor;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

layout (binding = 1) uniform sampler2D depthBuffer;
layout (binding = 2) uniform sampler2D normalBuffer;

layout(binding = 3) buffer samples_buf
{
    vec3 samples[];
};

layout (location = 0 ) in VS_OUT
{
    vec2 texCoord;
} surf;

vec4 toCam(vec2 sPos, float depth) {
    vec4 wPos = inverse(params.mProj) * vec4(2*sPos.xy - 1, depth, 1.0f);
    return wPos / wPos.w;
}

const float rad_coef = 0.5;

void main()
{
    vec2 coord = gl_FragCoord.xy/vec2(Params.screenWidth, Params.screenHeight);

    vec3 sPos = toCam(coord, textureLod(depthBuffer, coord, 0).x).xyz;
    if (-sPos.z > 50) {
        out_ssaoColor = 1.0;
        return;
    }
    vec3 normal = textureLod(normalBuffer, coord, 0).rgb;
    vec3 randomVec = vec3(0, 0, 1);

    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float ssao = 0;
    for (uint i = 0; i < Params.samples_count; i++) {
        vec3 cur = sPos + TBN * samples[i] * rad_coef;

        vec4 pPos = params.mProj * vec4(cur, 1.0);
        pPos /= pPos.w;
        pPos.xy = pPos.xy * 0.5 + 0.5;
        vec4 orig = toCam(pPos.xy, textureLod(depthBuffer, pPos.xy, 0).r);

        float diff = length(cur) - length(orig);
        if (diff > 0) {
            ssao += 1;
        }
        else {
            ssao += clamp(diff + 0.1, 0, 1);
        }
    }


    out_ssaoColor = 1.0 - ssao / Params.samples_count;
}