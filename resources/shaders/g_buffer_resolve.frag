#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

struct LightInfo{
    vec4 lightColor;
    vec3 lightPos;
    float radius;
};

layout(binding = 1, set = 0) buffer light_buf
{
    LightInfo lInfos[];
};

layout (location = 0) in GS_OUT
{
    uint light_ind;
} gOut;

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputDepth;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputNormal;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput inputAlbedo;
layout (input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput inputTangent;

void main()
{
    vec3 Normal = subpassLoad(inputNormal).rgb;
    vec3 Color = subpassLoad(inputAlbedo).rgb;
    vec3 Tangent = subpassLoad(inputTangent).rgb;

    //  / vec2(Params.screenWidth, Params.screenHeight)

    vec4 camPos = inverse(params.mProj) * vec4(2.0 * gl_FragCoord.xy - 1,
                                               subpassLoad(inputDepth).r, 1.0f);
    vec3 Pos = camPos.xyz / camPos.w;

    vec3 lightDir = normalize(vec3(params.mView * vec4(lInfos[gOut.light_ind].lightPos, 1.0f)) - Pos);
    vec4 lightColor = max(dot(Normal, lightDir), 0.0f) * lInfos[gOut.light_ind].lightColor;

    //vec3 lightDir = normalize(Params.lightPos - Pos);
    //vec4 lightColor = max(dot(Normal, lightDir), 0.0f) * vec4(0.59f, 0.0f, 0.82f, 1.0f);

    out_fragColor = lightColor * vec4(subpassLoad(inputAlbedo).rgb, 1.0f);
}