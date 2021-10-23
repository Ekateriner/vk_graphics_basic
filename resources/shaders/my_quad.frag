#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D colorTex;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} surf;

const int kernel_size = 3;
const int stride = 1;
const float k1 = 0.05;
const float h = 0.01;

void main()
{
  ivec2 tex_size = textureSize(colorTex, 0);
  float dx = (1.0 / tex_size.x);
  float dy = (1.0 / tex_size.y);
  vec4 clr = vec4(0.0);
  float sum = 0.0;

  vec4 center_clr = textureLod(colorTex, surf.texCoord, 0);
  //conv
  for (int i = -kernel_size; i < kernel_size; i += stride) {
    for (int j = -kernel_size; j < kernel_size; j += stride) {
      vec4 c = textureLod(colorTex, surf.texCoord + vec2(i*dx, j*dy), 0);
      float f = float(i) * float(i) + float(j) * float(j);
      float w = exp(-k1 * f);
      w *= exp(-length(c - center_clr) * length(c - center_clr) / h);

      clr += w*c;
      sum += w;
    }
  }

  color = clr / sum;
}
