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
    vec3 Pos;
    vec3 Norm;
    vec3 Tangent;
    vec2 texCoord;
} tcOut[];

layout (location = 0 ) out TES_OUT
{
    vec3 Norm;
    vec3 Tangent;
} teOut;

vec3 wind_dir = vec3(0.0, 0.0, 5.0);

void main() {
    mat4 mModel = params.mView * mat4(scale.x, 0, 0, 0,
                                      0, scale.y, 0, 0,
                                      0, 0, scale.z, 0,
                                      trans.x, trans.y, trans.z, 1.0);

    vec3 cPos = (gl_TessCoord.x * tcOut[0].Pos +
                 gl_TessCoord.y * tcOut[1].Pos +
                 gl_TessCoord.z * tcOut[2].Pos);

    float height_pos = cPos.y - (mModel * vec4(tcOut[0].texCoord.x, textureLod(heightMap, tcOut[0].texCoord.xy, 0).x, tcOut[0].texCoord.y, 1.0f)).y;

    teOut.Norm = tcOut[0].Norm;
    teOut.Tangent = tcOut[0].Tangent;
    cPos += pow(height_pos, 2) * wind_dir * sin(Params.time);

    gl_Position = params.mProj * vec4(cPos, 1.0f);
}
