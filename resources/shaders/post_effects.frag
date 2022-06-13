#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (binding = 1) uniform sampler2D inputColor;
layout (binding = 2) uniform sampler2D inputFog;
layout (binding = 3) uniform sampler2D inputSSAO;

layout (location = 0 ) in VS_OUT
{
    vec2 texCoord;
} surf;

const mat3 aces_input_matrix = mat3(0.59719f, 0.07600f, 0.02840f, \
                                    0.35458f, 0.90834f, 0.13383f, \
                                    0.04823f, 0.01566f, 0.83777f);

const mat3 aces_output_matrix = mat3(1.60475f, -0.10208f, -0.00327f, \
                                     -0.53108f, 1.10813f, -0.07276f, \
                                     -0.07367f, -0.00605f, 1.07602f);

vec3 rtt_and_odt_fit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 aces_fitted(vec3 v)
{
    v = aces_input_matrix * v;
    v = rtt_and_odt_fit(v);
    return aces_output_matrix * v;
}

void main()
{
//    vec2 TexCoord = surf.texCoord;
//    TexCoord.y = 1.0 - TexCoord.y;
    vec2 TexCoord = gl_FragCoord.xy/vec2(Params.screenWidth, Params.screenHeight);

    const int kernel_size = 6;
    const int stride = 1;
    const float k1 = 0.05;
    const float h = 0.01;

    const float dx = (1.0 / Params.screenWidth);
    const float dy = (1.0 / Params.screenHeight);

    // fog blur)
    vec4 fog = vec4(0);
    {
        float sum = 0.0;
        vec4 center_clr = textureLod(inputFog, TexCoord, 0);
        for (int i = -kernel_size / 2; i <= kernel_size / 2; i += stride) {
            for (int j = -kernel_size / 2; j <= kernel_size / 2; j += stride) {
                vec4 c = textureLod(inputFog, TexCoord + vec2(i*dx, j*dy), 0);
                float f = float(i) * float(i) + float(j) * float(j);
                float w = exp(-k1 * f);
                w *= exp(-length(c - center_clr) * length(c - center_clr) / h);

                fog += w*c;
                sum += w;
            }
        }
        fog = fog / sum;
    }

    // ssao blur)
    float ssao = 0;
    {
        float sum = 0.0;
        float center_clr = textureLod(inputSSAO, TexCoord, 0).r;
        for (int i = -kernel_size / 2; i <= kernel_size / 2; i += stride) {
            for (int j = -kernel_size / 2; j <= kernel_size / 2; j += stride) {
                float c = textureLod(inputSSAO, TexCoord + vec2(i*dx, j*dy), 0).r;
                float f = float(i) * float(i) + float(j) * float(j);
                float w = exp(-k1 * f);
                w *= exp(-length(c - center_clr) * length(c - center_clr) / h);

                ssao += w*c;
                sum += w;
            }
        }
        ssao = ssao / sum;
    }

    vec3 color = (1 - fog.a) * textureLod(inputColor, TexCoord, 0).xyz * ssao + fog.rgb;
    out_fragColor = vec4(aces_fitted(color), 1.0f);
}