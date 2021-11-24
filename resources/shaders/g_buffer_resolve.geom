#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

struct LightInfo{
    vec4 lightColor;
    vec3 lightPos;
    float radius;
};

layout(binding = 1, set = 0) buffer light_buf
{
    LightInfo lInfos[];
};

layout(push_constant) uniform params_t
{
    mat4 mProj;
    mat4 mView;
} params;


layout (location = 0) in VS_OUT
{
    uint light_ind;
} vOut[];

layout (location = 0) out GS_OUT
{
    uint light_ind;
} gOut;

vec3 proj (vec3 orig, vec3 base) {
    return base * dot(orig, base) / dot(base, base);
}

void GramShmidt(inout vec3 A, inout vec3 B, inout vec3 C){
    B = B - proj(B, A);
    C = C - proj(C, B) - proj(C, A);

    A = normalize(A);
    B = normalize(B);
    C = normalize(C);
}

void main() {
    gOut.light_ind = vOut[0].light_ind;
    LightInfo li = lInfos[gOut.light_ind];
    vec3 center = vec3(params.mView * vec4(li.lightPos, 1.0f));

    if(length(center) < li.radius) // inner
    {
        gl_Position = vec4(-1.0f, -1.0f, 0.5f, 1.0f);
        EmitVertex();
        gl_Position = vec4(-1.0f, 1.0f, 0.5f, 1.0f);
        EmitVertex();
        gl_Position = vec4(1.0f, -1.0f, 0.5f, 1.0f);
        EmitVertex();
        gl_Position = vec4(1.0f, 1.0f, 0.5f, 1.0f);
        EmitVertex();
        EndPrimitive();
    }
    else // outer
    {
        vec3 forward = center;
        vec3 right = forward + vec3(0.0f, 1.0f, 0.0f);
        vec3 up = forward + vec3(0.0f, 0.0f, 1.0f);

        GramShmidt(forward, right, up);

        float b = length(center);
        float radius2 = li.radius * sqrt((b + li.radius) / (b - li.radius));

        vec3 quad[4] = {
            - right - up,
            right - up,
            - right + up,
            right + up,
        };

        for (uint i = 0; i < 4; ++i)
        {
            gl_Position = params.mProj * vec4(forward + forward*li.radius + quad[i]*radius2, 1.0);
            EmitVertex();
        }

        EndPrimitive();
    }
}
