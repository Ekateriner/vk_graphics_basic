#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) in vec3 vPos;

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

layout(std430, binding = 3) buffer to_draw
{
    vec2 shifts_buf[]; // in tex_coord
};

layout (binding = 4) uniform sampler2D heightMap;

layout (binding = 5) uniform sampler2D grassMap;

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
    float gl_CullDistance[];
};

layout (location = 0 ) out VS_OUT
{
    vec3 Pos;
    vec3 Norm;
    vec3 Tangent;
    vec2 texCoord;
} vOut;

void main(void)
{
    //vec2 s = textureLod(grassMap, mod(shift.xy, 1.0f / tile_count) * tile_count, 0).xz;
    vec2 shift = shifts_buf[gl_InstanceIndex];
    vec2 coord = mod(shift.xy, 1.0f / tile_count) * tile_count * 0.5f;
    float rotation = textureLod(grassMap, coord + vec2(0.0f, 0.5f), 0).x * 2.0 * 3.1415926;
    mat3 rot_mat = mat3(cos(rotation), 0.0f, -sin(rotation),
                        0.0f, 1.0f, 0.0f,
                        sin(rotation), 0.0f, cos(rotation));

    vec2 s = vec2(textureLod(grassMap, coord, 0).x, textureLod(grassMap, coord + 0.5f, 0).x) - 0.5f;
    vOut.texCoord = shift + s / scale.xz;

    // borders
    vOut.texCoord.x = clamp(vOut.texCoord.x, 0.0f, 1.0f);
    vOut.texCoord.y = clamp(vOut.texCoord.y, 0.0f, 1.0f);

    mat4 mModel = mat4(scale.x, 0.0f, 0.0f, 0.0f,
                       0.0f, scale.y, 0.0f, 0.0f,
                       0.0f, 0.0f, scale.z, 0.0f,
                       trans.x, trans.y, trans.z, 1.0f);

    vec3 wShift    = vec3(vOut.texCoord.x, textureLod(heightMap, vOut.texCoord.xy, 0).x, vOut.texCoord.y);
    vOut.Pos       = rot_mat * vPos / scale.xyz + wShift;
    vOut.Norm      = normalize(mat3(transpose(inverse(params.mView * mModel))) * rot_mat * vec3(0.0f, 0.0f, 1.0f));
    vOut.Tangent   = normalize(mat3(transpose(inverse(params.mView * mModel))) * rot_mat * vec3(0.0f, 1.0f, 0.0f));

    gl_Position   = params.mProj * params.mView * mModel * vec4(vOut.Pos, 1.0f);
}
