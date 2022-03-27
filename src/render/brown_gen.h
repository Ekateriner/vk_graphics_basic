//
// Created by ekaterina on 21.03.2022.
//

#ifndef VK_GRAPHICS_BASIC_BROWN_GEN_H
#define VK_GRAPHICS_BASIC_BROWN_GEN_H

#include "LiteMath.h"
#include <random>

namespace BrownGenerator {
  static std::default_random_engine generator;
  static float rand_state = std::normal_distribution<float>(40000,6)(generator);
  static float x_coord = std::gamma_distribution<float>(5.0, 13.0)(generator);
  static float y_coord = std::gamma_distribution<float>(2.0, 75.0)(generator);
  
  inline float random (LiteMath::float2 st) {
    return LiteMath::fract(sin(LiteMath::dot(st,LiteMath::float2(x_coord,
                                                                 y_coord)))
                               * rand_state);
  }
  
  inline float noise (LiteMath::float2 st) {
    LiteMath::float2 i = floor(st);
    LiteMath::float2 f = fract(st);
    
    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + LiteMath::float2(1.0, 0.0));
    float c = random(i + LiteMath::float2(0.0, 1.0));
    float d = random(i + LiteMath::float2(1.0, 1.0));
    
    LiteMath::float2 u = f * f * (3.0 - 2.0 * f);
    
    return LiteMath::mix(a, b, u.x) +
           (c - a)* u.y * (1.0 - u.x) +
           (d - b) * u.x * u.y;
  }

  #define OCTAVES 6
  
  inline float fbm (LiteMath::float2 st) {
    // Initial values
    float value = 0.0;
    float amplitude = 0.5;
    //float frequency = 0.0;
    //
    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++) {
      value += amplitude * noise(st);
      st *= 2.;
      amplitude *= .5;
    }
    return value;
  }
  
  void regenerate() {
    rand_state = std::normal_distribution<float>(40000,6)(generator);
    x_coord = std::gamma_distribution<float>(5.0, 13.0)(generator);
    y_coord = std::gamma_distribution<float>(2.0, 75.0)(generator);
  }
}

#endif //VK_GRAPHICS_BASIC_BROWN_GEN_H
