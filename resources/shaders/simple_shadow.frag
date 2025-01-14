#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common_withLight.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParamsL Params;
};

layout (binding = 1) uniform sampler2D shadowMap;

const float trans_coef = 0.5;

float width(vec3 pos, vec3 N){
    vec4 shrinkedpos = vec4(pos - 0.005f * N, 1.0);
    vec4 shwpos = shrinkedpos * Params.lightMatrix;
    float d1 = textureLod(shadowMap, shwpos.xy/shwpos.w, 0).x;
    float d2 = shwpos.z;
    return abs(d1 - d2);
}

// This function can be precomputed for efficiency
vec3 T(float s) {
    return vec3(0.233, 0.455, 0.649) * exp(-s*s/0.0064) + \
           vec3(0.1, 0.336, 0.344) * exp(-s*s/0.0484) + \
           vec3(0.118, 0.198, 0.0) * exp(-s*s/0.187) + \
           vec3(0.113, 0.007, 0.007) * exp(-s*s/0.567) + \
           vec3(0.358, 0.004, 0.0) * exp(-s*s/1.99) + \
           vec3(0.078, 0.0, 0.0) * exp(-s*s/7.41);
}

void main()
{
  const vec4 posLightClipSpace = Params.lightMatrix*vec4(surf.wPos, 1.0f); // 
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]               

  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float mu = textureLod(shadowMap, shadowTexCoord, 0).x;
  const float sigma = textureLod(shadowMap, shadowTexCoord, 0).y - mu * mu;
  const float soft_shadow = (!outOfView && Params.vsm == 1) ? max(sigma / (sigma + (posLightSpaceNDC.z - mu) * (posLightSpaceNDC.z - mu)), 0.1f) : 0.0f;
  const float shadow    = ((posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : soft_shadow;

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

  vec3 color;
  if (Params.animateLightColor != 0) {
    color = mix(dark_violet, chartreuse, sin(Params.time)).xyz;
  }
  else {
    color = mix(dark_violet, chartreuse, 0.5f).xyz;
  }
  vec4 lightColor = vec4(color, 1.0f); // in frag

  if (Params.flashLight != 0) {
    //lightColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
    vec3 lightDir   = normalize(surf.wPos - Params.lightPos);
    vec3 flashDir = normalize(Params.flashDir);
    float angle = acos(dot(flashDir, lightDir));
    if (angle <= Params.flashMinMaxRadius.x) {
      out_fragColor = max(dot(surf.wNorm, -lightDir), 0.0f) * (lightColor*shadow + vec4(0.1f)) * vec4(Params.baseColor, 1.0f);
    }
    else if (angle <= Params.flashMinMaxRadius.y){
      lightColor = max(dot(surf.wNorm, -lightDir), 0.0f) * (Params.flashMinMaxRadius.y - angle) / (Params.flashMinMaxRadius.y - Params.flashMinMaxRadius.x) * lightColor;
      out_fragColor   = (lightColor*shadow + vec4(0.1f)) * vec4(Params.baseColor, 1.0f);
    }
    else {
      lightColor = 0.000001f * max(dot(surf.wNorm, -lightDir), 0.0f) * lightColor;
      out_fragColor   = (lightColor*shadow + vec4(0.1f)) * vec4(Params.baseColor, 1.0f);
    }

    if (length(Params.lightPos - surf.wPos) >= Params.flashMinMaxRadius.z) {
      float len = length(Params.lightPos - surf.wPos);
      out_fragColor *= (2.25f * Params.flashMinMaxRadius.z * Params.flashMinMaxRadius.z - len * len) / (1.25f * Params.flashMinMaxRadius.z * Params.flashMinMaxRadius.z);
    }
    if (length(Params.lightPos - surf.wPos) >= 1.5f*Params.flashMinMaxRadius.z) {
      out_fragColor *= 0.000001f;
    }
  }
  else {
    vec3 lightDir   = normalize(Params.lightPos - surf.wPos);
    lightColor = max(dot(surf.wNorm, lightDir), 0.0f) * lightColor;

    float s = width(surf.wPos, surf.wNorm);
    float E = max(0.3 + dot(-surf.wNorm, lightDir), 0.0);
    vec3 transmittance = T(s) * color.rgb * Params.baseColor * E * trans_coef;

    vec3 M = (lightColor*shadow + vec4(0.1f)).rgb * Params.baseColor.rgb;
    if (Params.sss != 0) {
      M += transmittance;
    }

    out_fragColor = vec4(M, 1.0);

    //out_fragColor   = (lightColor*shadow + vec4(0.1f)) * vec4(Params.baseColor, 1.0f);
  }
}
