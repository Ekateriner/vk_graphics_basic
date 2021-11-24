#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "../../../../resources/shaders/common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

float fog_density(float z) {
    return 0.03 * min(z, 100);
}

void main()
{
    vec3 lightDir1 = normalize(Params.lightPos - surf.wPos);
    vec3 lightDir2 = vec3(0.0f, 0.0f, 1.0f);

    const vec4 dark_violet = vec4(0.75f, 1.0f, 0.25f, 1.0f);
    const vec4 chartreuse  = vec4(0.75f, 1.0f, 0.25f, 1.0f);

    vec4 lightColor1 = mix(dark_violet, chartreuse, 0.5f);
    if(Params.animateLightColor)
        lightColor1 = mix(dark_violet, chartreuse, abs(sin(Params.time)));

    vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);

    vec3 N = surf.wNorm; 

    vec4 color1 = max(dot(N, lightDir1), 0.0f) * lightColor1;
    vec4 color2 = max(dot(N, lightDir2), 0.0f) * lightColor2;
    vec4 color_lights = mix(color1, color2, 0.2f);

    float sin_time = sin(Params.time) * 0.1;

    vec3 time_dependent_color = vec3(sin_time + 0.1, max(sin(Params.time * 1.231) - 0.99, 0) * 30, -sin_time + 0.1);

    //are equal
//    if (N.y > 0.5) {
//        time_dependent_color = vec3(sin(Params.time) * 0.1 + 0.1, max(sin(Params.time * 1.231) - 0.99, 0) * 30, -sin(Params.time) * 0.1 + 0.1);
//    } else {
//        time_dependent_color = vec3(-cos(Params.time + 3.14 / 2.0) * 0.1 + 0.1, max(sin(Params.time * 1.231) - 0.99, 0) / 0.01 * 0.3, cos(Params.time + 3.14 / 2.0) * 0.1 + 0.1);
//    }
    // zero part
//    if (N.x < 0) {
//        for (int i = 0; i < 300; ++i) {
//            time_dependent_color.x += 1.0 / pow(max(fract(Params.time), 3.0 + i), 7000);
//        }
//    } else {
//        for (int i = 0; i < 400; ++i) {
//            time_dependent_color.x += 1.0 / pow(max(fract(Params.time + 30) - 0.3, 64.0 + i), 4000);
//        }
//    }

    vec3 abrakadabra = abs(fract(surf.wPos) * 2.0 - 1.0);
    time_dependent_color *= pow(max(abrakadabra.x, max(abrakadabra.y, abrakadabra.z)), 30);

    vec3 screenFog = mix(vec3(1, 0, 0), vec3(0, 0, 1), gl_FragCoord.x / 1024) * fog_density(1 / gl_FragCoord.w);

    out_fragColor = color_lights * vec4(Params.baseColor, 1.0f) + vec4(time_dependent_color + vec3(0, 0, 0.1) + screenFog, 0);
}