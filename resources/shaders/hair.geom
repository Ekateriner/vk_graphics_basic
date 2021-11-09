#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(invocations = 2) in;

layout(triangles) in;
layout(triangle_strip, max_vertices=4) out;

layout (location = 0 ) in GS_IN
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vIn[];

layout (location = 0 ) out GS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
} params;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

const float length = 0.3f;
const float width = 0.0001f;

void main() {

    if (gl_InvocationID.x != 0) {
        vec3 center = (vIn[0].wPos + vIn[1].wPos + vIn[2].wPos) / 3;
        vec3 dist = vIn[0].wTangent;
        vec3 norm = cross(vIn[0].wNorm, vIn[0].wTangent);
        vec3 tangent = vIn[0].wNorm;
        
        vOut.wPos = center - width * dist;
        vOut.wNorm = norm;
        vOut.wTangent = tangent;
        gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
        EmitVertex();

        vOut.wPos = center + width * dist;
        vOut.wNorm = norm;
        vOut.wTangent = tangent;
        gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
        EmitVertex();

        vOut.wPos = center + length * vIn[0].wNorm - width * dist;
        vOut.wNorm = norm;
        vOut.wTangent = tangent;
        gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
        EmitVertex();

        vOut.wPos = center + length * vIn[0].wNorm + width * dist;
        vOut.wNorm = norm;
        vOut.wTangent = tangent;
        gl_Position = params.mProjView * vec4(vOut.wPos, 1.0);
        EmitVertex();

        EndPrimitive();//hair
    }
    else {
        for (int i = 0; i < 3; i++) {
            gl_Position = gl_in[i].gl_Position;

            vOut.wPos = vIn[i].wPos;
            vOut.wNorm = vIn[i].wNorm;
            vOut.wTangent = vIn[i].wTangent;
            vOut.texCoord = vIn[i].texCoord;
            EmitVertex();
        }
        EndPrimitive();// basic
    }
}
