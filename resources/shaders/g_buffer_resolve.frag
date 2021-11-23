#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout(push_constant) uniform params_t
{
    mat4 mProjView;
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

    vec4 camPos = inverse(params.mProjView) * vec4(2.0 * gl_FragCoord.xy / vec2(Params.screenWidth, Params.screenHeight) - 1, subpassLoad(inputDepth).r, 1.0f);
    vec3 Pos = camPos.xyz / camPos.w;

    vec3 lightDir = normalize(Params.lightPos - Pos);

    const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
    const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

    vec4 lightColor = mix(dark_violet, chartreuse, 0.5f);
    if(Params.animateLightColor)
        lightColor = mix(dark_violet, chartreuse, abs(sin(Params.time)));

    lightColor = max(dot(Normal, lightDir), 0.0f) * lightColor;

    out_fragColor = lightColor * vec4(subpassLoad(inputAlbedo).rgb, 1.0f);
}