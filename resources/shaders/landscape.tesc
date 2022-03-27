#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout ( vertices = 4 ) out;

layout(binding = 1, set = 0) uniform land_info
{
    vec4 scale;
    vec4 trans;
    vec4 sun_pos;
    int width;
    int height;
};

void main() {
    gl_out [gl_InvocationID].gl_Position = gl_in [gl_InvocationID].gl_Position;

    if ( gl_InvocationID == 0 )         // set tessellation level, can do only for one vertex
    {
        gl_TessLevelInner [0] = width;
        gl_TessLevelInner [1] = height;

        gl_TessLevelOuter [0] = width;
        gl_TessLevelOuter [1] = height;
        gl_TessLevelOuter [2] = width;
        gl_TessLevelOuter [3] = height;
    }
}
