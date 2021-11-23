#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

void main() {
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
