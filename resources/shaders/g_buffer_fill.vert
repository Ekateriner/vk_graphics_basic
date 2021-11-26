#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(std430, binding = 1) buffer to_draw
{
    mat4 drawMatrices_buf[];
};

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 Norm;
    vec3 Tangent;
    //vec2 texCoord;
} vOut;

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
    float gl_CullDistance[];
};

void main(void)
{
    mat4 mModel = params.mView * drawMatrices_buf[gl_InstanceIndex];
    const vec4 Norm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 Tang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    //vOut.Pos     = (mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.Norm    = normalize(mat3(transpose(inverse(mModel))) * Norm.xyz);
    vOut.Tangent = normalize(mat3(transpose(inverse(mModel))) * Tang.xyz);

    gl_Position   = params.mProj * mModel * vec4(vPosNorm.xyz, 1.0f);
}
