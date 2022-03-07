#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common_withLight.h"

layout (location = 0) out vec2 out_depthColor;

layout (location = 0 ) in VS_OUT
{
    vec2 texCoord;
} surf;

layout (binding = 0) uniform sampler2D shadowMap;

const int kernel_size = 15;
const int stride = 1;

void main()
{
    vec2 shadowTexCoord = surf.texCoord;
    shadowTexCoord.y = 1.0 - shadowTexCoord.y;

    ivec2 tex_size = textureSize(shadowMap, 0);
    float dx = (1.0 / tex_size.x);
    float dy = (1.0 / tex_size.y);
    float clr = 0.0;
    float clr2 = 0.0;
    float sum = 0.0;

    //float center_clr = textureLod(shadowMap, shadowTexCoord, 0).x;
    //float center_clr2 = pow(textureLod(shadowMap, shadowTexCoord, 0).x, 2.0);
    //conv
    for (int i = -kernel_size / 2; i <= kernel_size / 2; i += stride) {
        for (int j = -kernel_size / 2; j <= kernel_size / 2; j += stride) {
            clr += textureLod(shadowMap, shadowTexCoord + vec2(i*dx, j*dy), 0).x;
            clr2 += pow(textureLod(shadowMap, shadowTexCoord + vec2(i*dx, j*dy), 0).x, 2.0);

            sum += 1;
        }
    }

    out_depthColor = vec2(clr / sum, clr2 / sum);
}



