#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common.h"

layout (quads, equal_spacing) in;

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

layout (binding = 2) uniform sampler2D heightMap;

vec3 getNorm(vec3 Pos) {
    float R = textureLod(heightMap, gl_TessCoord.xy + vec2(1.0f/width, 0.0), 0).x;
    float L = textureLod(heightMap, gl_TessCoord.xy - vec2(1.0f/width, 0.0), 0).x;
    float U = textureLod(heightMap, gl_TessCoord.xy + vec2(0.0f, 1.0f/height), 0).x;
    float D = textureLod(heightMap, gl_TessCoord.xy - vec2(0.0f, 1.0f/height), 0).x;

    return normalize(vec3(R - L, 1.0, D - U) / vec3(2.0f/width, 1.0, 2.0/height));
}

layout (location = 0 ) out VS_OUT
{
    vec3 Pos; //tex space
    vec3 Norm;
    vec3 Tangent;
    //vec2 texCoord;
} vOut;

void main() {

    //gl_Position = textureLod(heightMap, gl_TessCoord.xz, 0).x;
    mat4 mModel = params.mView * mat4(scale.x, 0, 0, 0,
                                      0, scale.y, 0, 0,
                                      0, 0, scale.z, 0,
                                      trans.x, trans.y, trans.z, 1.0);
    const vec3 Pos = vec3(gl_TessCoord.x, textureLod(heightMap, gl_TessCoord.xy, 0).x, gl_TessCoord.y);
    const vec3 Norm = getNorm(Pos);
    const vec3 Tang = vec3(0.0);

    vOut.Pos     = Pos;
    vOut.Norm    = normalize(mat3(transpose(inverse(mModel))) * Norm.xyz);
    vOut.Tangent = normalize(mat3(transpose(inverse(mModel))) * Tang.xyz);
    //vOut.texCoord = gl_TessCoord.xy;

    gl_Position   = params.mProj * mModel * vec4(Pos, 1.0f);
}
