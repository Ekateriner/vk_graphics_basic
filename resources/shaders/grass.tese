#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common.h"

layout (triangles, equal_spacing) in;

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

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

layout (binding = 4) uniform sampler2D heightMap;

layout (binding = 5) uniform sampler2D grassMap;

layout (location = 0 ) in TCS_OUT
{
    vec2 texCoord;
} tcOut[];

void main() {
//    vec3 p0 = gl_TessCoord.x * tcPosition[0];
//    vec3 p1 = gl_TessCoord.y * tcPosition[1];
//    vec3 p2 = gl_TessCoord.z * tcPosition[2];
//
//    tePosition = normalize(p0 + p1 + p2);
    vec3 cPos = gl_TessCoord.xyz;

    gl_Position = params.mProj *  (gl_TessCoord.x * gl_in [0].gl_Position +
                                   gl_TessCoord.y * gl_in [1].gl_Position +
                                   gl_TessCoord.z * gl_in [2].gl_Position);
}
