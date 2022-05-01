#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_normalColor;
layout(location = 1) out vec4 out_albedoColor;
layout(location = 2) out vec4 out_tangentColor;

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

layout (location = 0 ) in VS_OUT
{
    vec3 Pos; // tex space
    vec3 Norm;
    vec3 Tangent;
    //vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout(binding = 1, set = 0) uniform land_info
{
    vec4 scale;
    vec4 trans;
    vec4 sun_pos;
    int width;
    int height;
};

layout (binding = 2) uniform sampler2D heightMap;

float eps = 0.001f;
int hits = 20;

void main()
{
    mat4 mModel = mat4(scale.x, 0, 0, 0,
                       0, scale.y, 0, 0,
                       0, 0, scale.z, 0,
                       trans.x, trans.y, trans.z, 1.0);

    out_normalColor = vec4(surf.Norm, 1.0f);
    out_albedoColor = vec4(Params.baseColor, 1.0f);
    out_tangentColor = vec4(surf.Tangent, 1.0f);

    vec3 SunPos = (inverse(mModel) * sun_pos).xyz; //tex_space
    vec3 step = normalize(SunPos - surf.Pos) * eps; //tex space
    vec3 cur =  surf.Pos + step;
    int shadow = 0;
    while (length(cur - surf.Pos) < length(SunPos - surf.Pos)) {
        if (cur.y < textureLod(heightMap, cur.xz, 0).x) {
            shadow += 1;
        }
        if (cur.x > 1.0f || cur.x < 0.0f || cur.y > 1.0f || cur.y < 0.0f || cur.z > 1.0f || shadow >= hits) {
            break;
        }
        cur += step;
    }
    out_albedoColor = vec4(Params.baseColor, float(hits - shadow) / hits);
}