#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(invocations = 2) in;

layout(triangles) in;
layout(triangle_strip, max_vertices=3) out;

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

const float length = 1.0f;
const float width = 0.1f;

void main() {
    if (gl_InvocationID.x == 0) {
        vec3 center = (vIn[0].wPos + vIn[1].wPos + vIn[2].wPos) / 3;
        vOut.wPos = center - width * vIn[0].wTangent;
        vOut.wNorm = cross(vIn[0].wNorm, vIn[0].wTangent);
        vOut.wTangent = vIn[0].wNorm;
        EmitVertex();
        vOut.wPos = center - width * vIn[0].wTangent;
        vOut.wNorm = cross(vIn[0].wNorm, vIn[0].wTangent);
        vOut.wTangent = vIn[0].wNorm;
        EmitVertex();
        vOut.wPos = center + length * vIn[0].wNorm;
        vOut.wNorm = cross(vIn[0].wNorm, vIn[0].wTangent);
        vOut.wTangent = vIn[0].wNorm;
        EmitVertex();
        EndPrimitive();//hair
    }
    else {
        for (int i = 0; i < 3; i++) {
            vOut.wPos = vIn[i].wPos;
            vOut.wNorm = vIn[i].wNorm;
            vOut.wTangent = vIn[i].wTangent;
            vOut.texCoord = vIn[i].texCoord;
            EmitVertex();
        }
        EndPrimitive();// basic
    }
}
