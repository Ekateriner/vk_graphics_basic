//
// Created by ekaterina on 21.03.2022.
//

#ifndef VK_GRAPHICS_BASIC_SPHERE_GEN_H
#define VK_GRAPHICS_BASIC_SPHERE_GEN_H

#include "LiteMath.h"
#include <random>

namespace SphereGenerator {
  static std::default_random_engine generator;
  
  inline LiteMath::float3 gen() {
    float x_coord = std::normal_distribution<float>(0.5)(generator);
    float y_coord = std::normal_distribution<float>(0.5)(generator);
    float z_coord = std::gamma_distribution<float>(3.0, 0.5)(generator);
    
    return LiteMath::float3(x_coord, y_coord, z_coord);
  }
}

#endif //VK_GRAPHICS_BASIC_SPHERE_GEN_H
