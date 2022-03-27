#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"

out gl_PerVertex {
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
    float gl_CullDistance[];
};

void main(void)
{
    if (gl_VertexIndex == 0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
    }
    else if (gl_VertexIndex == 1) {
        gl_Position = vec4(0.0, 1.0, 0.0, 0.0);
    }
    else if (gl_VertexIndex == 2) {
        gl_Position = vec4(1.0, 0.0, 0.0, 0.0);
    }
    else if (gl_VertexIndex == 3) {
        gl_Position = vec4(1.0, 1.0, 0.0, 0.0);
    }
}
