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

layout (location = 0) in flat uint light_ind;

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

layout (input_attachment_index = 0, set = 1, binding = 0) uniform subpassInput inputDepth;
layout (input_attachment_index = 1, set = 1, binding = 1) uniform subpassInput inputNormal;
layout (input_attachment_index = 2, set = 1, binding = 2) uniform subpassInput inputAlbedo;
layout (input_attachment_index = 3, set = 1, binding = 3) uniform subpassInput inputTangent;

const float lightSize = 0.1;

float pow2 (float x) {
    return x * x;
}

void main()
{
    vec3 Normal = subpassLoad(inputNormal).rgb;
    vec3 Color = subpassLoad(inputAlbedo).rgb;
    vec3 Tangent = subpassLoad(inputTangent).rgb;

    LightInfo li = lInfos[light_ind];

    //  / vec2(Params.screenWidth, Params.screenHeight)
    vec4 camPos = inverse(params.mProj) * vec4(2.0 * gl_FragCoord.xy / vec2(Params.screenWidth, Params.screenHeight) - 1,
                                               subpassLoad(inputDepth).r, 1.0f);
    vec3 Pos = camPos.xyz / camPos.w;

    vec3 lightDir = (params.mView * vec4(li.lightPos, 1.0f)).xyz - Pos;
    li.lightPos.xy = gl_FragCoord.xy;

    // красивый затухающий свет
    float lightDist2 = dot(lightDir, lightDir);
    lightDir = normalize(lightDir);

    float lightRadiusMin2 = lightSize * lightSize;
    float lightRadiusMax2 = li.radius * li.radius;

    float lightSampleDist = mix(lightRadiusMin2, lightRadiusMax2, 0.05);
    float attenuation = lightSampleDist / max(lightRadiusMin2, lightDist2) * pow2(max(1 - pow2(lightDist2 / lightRadiusMax2), 0));

    vec3 lightDiffuse = max(dot(Normal, lightDir), 0.0f) * li.lightColor.rgb;

    out_fragColor = vec4(lightDiffuse * subpassLoad(inputAlbedo).rgb, attenuation);
}