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
const vec3 ambient = vec3(0.1);
//const float trans_coef = 0.5;
//
//float distance(vec3 pos, vec3 N, int i){
//    vec4 shrinkedpos = vec4(pos - 0.005f * N, 1.0);
//    vec4 shwpos = mul(shrinkedpos, lights[i].viewproj);
//    float d1 = shwmaps[i].Sample(sampler, shwpos.xy/shwpos.w);
//    float d2 = shwpos.z;
//    return abs(d1 - d2);
//}
//
//// This function can be precomputed for efficiency
//vec3 T(float s) {
//    return vec3(0.233, 0.455, 0.649) * exp(-s*s/0.0064) + \
//           vec3(0.1, 0.336, 0.344) * exp(-s*s/0.0484) + \
//           vec3(0.118, 0.198, 0.0) * exp(-s*s/0.187) + \
//           vec3(0.113, 0.007, 0.007) * exp(-s*s/0.567) + \
//           vec3(0.358, 0.004, 0.0) * exp(-s*s/1.99) + \
//           vec3(0.078, 0.0, 0.0) * exp(-s*s/7.41);
//}

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
    //li.lightPos.xy = gl_FragCoord.xy;

    if (li.radius < 0.001f) {
        lightDir = normalize(lightDir);

//        float s = scale * distance(pos, Normal, light_ind);
//        float E = max(0.3 + dot(-Normal, lightDir), 0.0);
//        vec3 transmittance = T(s) * li.lightColor.rgb * Color * E * trans_coef;
        vec3 lightDiffuse = max(dot(Normal, lightDir), 0.0f) * li.lightColor.rgb;
        vec3 M = (lightDiffuse * subpassLoad(inputAlbedo).w + ambient) * Color;// + transmittance;

        out_fragColor = vec4(M, 0.5);

        if (subpassLoad(inputDepth).r == 1) {
            out_fragColor = vec4(0.1, 0.2, 0.3, 1.0);
        }
    }
    else {
        // красивый затухающий свет
        float lightDist2 = dot(lightDir, lightDir);
        lightDir = normalize(lightDir);

        float lightRadiusMin2 = lightSize * lightSize;
        float lightRadiusMax2 = li.radius * li.radius;

        float lightSampleDist = mix(lightRadiusMin2, lightRadiusMax2, 0.05);
        float attenuation = lightSampleDist / max(lightRadiusMin2, lightDist2) * pow2(max(1 - pow2(lightDist2 / lightRadiusMax2), 0));

        vec3 lightDiffuse = max(dot(Normal, lightDir), 0.0f) * li.lightColor.rgb;

        out_fragColor = vec4(lightDiffuse * Color, attenuation);
    }
}