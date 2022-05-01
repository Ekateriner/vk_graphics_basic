#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 out_fragColor;

layout (binding = 0) uniform sampler2D inputColor;

layout (location = 0 ) in VS_OUT
{
    vec2 texCoord;
} surf;

const mat3 aces_input_matrix = mat3(0.59719f, 0.07600f, 0.02840f, \
                                    0.35458f, 0.90834f, 0.13383f, \
                                    0.04823f, 0.01566f, 0.83777f);

const mat3 aces_output_matrix = mat3(1.60475f, -0.10208f, -0.00327f, \
                                     -0.53108f, 1.10813f, -0.07276f, \
                                     -0.07367f, -0.00605f, 1.07602f);

vec3 rtt_and_odt_fit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 aces_fitted(vec3 v)
{
    v = aces_input_matrix * v;
    v = rtt_and_odt_fit(v);
    return aces_output_matrix * v;
}

void main()
{
    vec2 TexCoord = surf.texCoord;
    TexCoord.y = 1.0 - TexCoord.y;

    out_fragColor = vec4(aces_fitted(textureLod(inputColor, TexCoord, 0).xyz), 1.0f);
}