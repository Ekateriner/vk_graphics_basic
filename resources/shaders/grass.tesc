#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout ( vertices = 3 ) out;

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

layout(binding = 2, set = 0) uniform grass_info
{
    uvec2 tile_count;
    vec2 nearfar;
    uvec2 freq_min;
    uvec2 freq_max;
};

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

layout (location = 0 ) in VS_OUT
{
    vec3 Pos;
    vec3 Norm;
    vec3 Tangent;
    vec2 texCoord;
} vOut[];

layout (location = 0 ) out TCS_OUT
{
    vec3 Pos;
    vec3 Norm;
    vec3 Tangent;
    vec2 texCoord;
} tcOut[];


void main() {
    gl_out [gl_InvocationID].gl_Position = gl_in [gl_InvocationID].gl_Position;

    tcOut[gl_InvocationID].Pos = vOut[gl_InvocationID].Pos;
    tcOut[gl_InvocationID].Norm = vOut[gl_InvocationID].Norm;
    tcOut[gl_InvocationID].Tangent = vOut[gl_InvocationID].Tangent;
    tcOut[gl_InvocationID].texCoord = vOut[gl_InvocationID].texCoord;
    if ( gl_InvocationID == 0 )         // set tessellation level, can do only for one vertex
    {
        float depth = length(gl_in [gl_InvocationID].gl_Position.xyz);
        if (depth <= nearfar.x) {
            gl_TessLevelInner [0] = 2;
            //gl_TessLevelInner [1] = 3;

            gl_TessLevelOuter [0] = 1;
            gl_TessLevelOuter [1] = 5;
            gl_TessLevelOuter [2] = 5;
            //gl_TessLevelOuter [3] = 5;
        }
        else if (depth <= nearfar.y) {
            gl_TessLevelInner [0] = 1;
            //gl_TessLevelInner [1] = 2;

            gl_TessLevelOuter [0] = 1;
            gl_TessLevelOuter [1] = 3;
            gl_TessLevelOuter [2] = 3;
            //gl_TessLevelOuter [3] = 4;
        }
        else {
            gl_TessLevelInner [0] = 1;
            //gl_TessLevelInner [1] = 1;

            gl_TessLevelOuter [0] = 1;
            gl_TessLevelOuter [1] = 1;
            gl_TessLevelOuter [2] = 1;
            //gl_TessLevelOuter [3] = 3;
        }
    }
}
