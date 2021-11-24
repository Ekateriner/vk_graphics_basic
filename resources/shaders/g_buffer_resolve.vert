#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

layout (location = 0) out VS_OUT
{
    uint light_ind;
} vOut;

void main(void)
{
    vOut.light_ind = gl_InstanceIndex.x;
    gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}
