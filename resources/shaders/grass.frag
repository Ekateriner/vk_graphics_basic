#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_normalColor;
layout(location = 1) out vec4 out_albedoColor;
layout(location = 2) out vec4 out_tangentColor;
//
//layout (location = 0 ) in VS_OUT
//{
//    vec3 wPos;
//} vOut;
//
//layout(binding = 0, set = 0) uniform AppData
//{
//    UniformParams Params;
//};

void main()
{
    out_normalColor = vec4(0.0, 0.0, 1.0, 1.0f);
    out_albedoColor = vec4(0.0, 0.2, 0.0, 1.0f);
    out_tangentColor = vec4(0.0, 1.0, 0.0, 1.0f);
}