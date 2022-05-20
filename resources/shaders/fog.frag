#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"
#include "fog.glsl"

layout(location = 0) out vec4 out_fragColor;

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

layout(binding = 2, set = 0) uniform land_info
{
    vec4 scale;
    vec4 trans;
    vec4 sun_pos;
    int width;
    int height;
};

layout (binding = 3, set = 0) uniform sampler2D heightMap;

vec4 toWorld(vec2 sPos, float depth) {
    vec4 wPos = inverse(params.mProj*params.mView) * vec4(2*sPos.xy - 1, depth, 1.0f);
    return wPos / wPos.w;
}

vec4 toScreen(vec3 wPos) {
    return params.mProj*params.mView * vec4(wPos, 1.0f);
}

float toTex(float height) {
    return (height - trans.y) / scale.y;
}

layout (location = 0 ) in VS_OUT
{
    vec2 texCoord;
} surf;

float eps = 0.01f;
uint hits = 35;

const int step_count = 50;

void main()
{
//    vec2 TexCoord = surf.texCoord;
//    TexCoord.y = 1.0 - TexCoord.y;
    mat4 mModel = mat4(scale.x, 0.0f, 0.0f, 0.0f,
                       0.0f, scale.y, 0.0f, 0.0f,
                       0.0f, 0.0f, scale.z, 0.0f,
                       trans.x, trans.y, trans.z, 1.0f);

    vec3 SunPos = (inverse(mModel) * sun_pos).xyz; //tex_space

    vec2 coord = gl_FragCoord.xy/vec2(Params.screenWidth, Params.screenHeight) * Params.fog_scale;

    vec4 wEndPos = toWorld(coord, textureLod(depthBuffer, coord, 0).x);
    vec4 wBeginPos = toWorld(vec2(0.5,0.5),0.0);

    const vec3 wDir = normalize(wEndPos - wBeginPos).xyz;

    const float stepLen = 0.8f;
    const float fog_height_coef = 0.5;
    const float fog_height_min = scale.y * 0.45f + trans.y;
    const float fog_height_max = 0.5f; //blur line

    vec3 wCurrent = toWorld(coord, 0).xyz;
    vec4 color = vec4(0.0, 0.0, 0.0, 1.0);

    for (int i = 0; i < step_count; i++) {
        //out_fragColor = vec4(clamp((fog_height_max-wEndPos.y) / (fog_height_max - fog_height_min), 0, 1), 0, 0, 1.0);
        if (color.a < 0.3 || dot(wEndPos.xyz - wCurrent, wDir) < 0) {
            break;
        }

        //float f = clamp(exp(-wCurrent.y / fog_height_max), 0, 1) * clamp((density(wCurrent / 10, Params.time) + 1) * 0.015, 0, 1);
        //float f = clamp((fog_height_max-wCurrent.y) / fog_height_coef, 0, 1);
        float f = (clamp(exp((fog_height_max-toTex(wCurrent.y)) * fog_height_coef), 0, 1) * 0.5f  \
                 - (density((wCurrent / 5.0 - Params.time / 5.0 * vec3(1, 0, 1)) / 2.0, Params.time * 2.0) + 1.0) * 0.35f) * 3.5;
        f = clamp(f, 0, 1);
        const float beersTerm = exp(-f*stepLen * f*stepLen);

        // calc shadow
        vec3 surf_pos = (inverse(mModel) * vec4(wCurrent, 1.0f)).xyz;
        vec3 step = normalize(SunPos - surf_pos) * eps; //tex space
        vec3 cur =  surf_pos + step;
        uint shadow = 0;
        while (length(cur - surf_pos) < length(SunPos - surf_pos)) {
            if (cur.y < textureLod(heightMap, cur.xz, 0).x) {
                shadow += 1;
            }
            if (cur.x > 1.0f || cur.x < 0.0f || cur.y > 1.0f || cur.y < 0.0f || cur.z > 1.0f || shadow >= hits) {
                break;
            }
            cur += step;
        }

        const float shadow_coef = float(hits - shadow) / float(hits);
        color.rgb += (1 - beersTerm) * color.a * (shadow_coef * 0.8 + (1 - shadow_coef) * vec3(0.1));
        color.a *= beersTerm;
        wCurrent += wDir * stepLen;
    }
    //out_fragColor = vec4(toTex(wEndPos.y), 0, 0, 1);
    out_fragColor = vec4(color.rgb, 1.0 - color.a);
}