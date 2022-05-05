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
    vec2 texCoord;
} vOut;

void main(void)
{
    //vec2 s = textureLod(grassMap, mod(shift.xy, 1.0f / tile_count) * tile_count, 0).xz;
    vec2 shift = shifts_buf[gl_InstanceIndex];
    vec2 s = textureLod(grassMap, mod(shift.xy, 1.0f / tile_count) * tile_count, 0).xy - 0.5;
    vOut.texCoord = shift + s / scale.xz;

    mat4 mModel = params.mView * mat4(scale.x, 0, 0, 0,
                                      0, scale.y, 0, 0,
                                      0, 0, scale.z, 0,
                                      trans.x, trans.y, trans.z, 1.0);

    vec3 cShift    = (mModel * vec4(vOut.texCoord.x, textureLod(heightMap, vOut.texCoord.xy, 0).x, vOut.texCoord.y, 1.0f)).xyz;
    vec3 cPos      = vPos + cShift;

    gl_Position   = vec4(cPos, 1.0);
}
