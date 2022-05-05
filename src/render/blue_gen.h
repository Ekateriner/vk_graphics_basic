//
// Created by ekaterina on 21.03.2022.
//

#ifndef VK_GRAPHICS_BASIC_BLUE_GEN_H
#define VK_GRAPHICS_BASIC_BLUE_GEN_H

#include "LiteMath.h"
#include <random>
#include <iostream>

namespace BlueGenerator { //loads random part of texture
  static unsigned tex_W = 0;
  static unsigned tex_H = 0;
  
  static std::vector<unsigned> LoadBMP(const char* filename)
  {
    FILE* f = fopen(filename, "rb");
    
    if(f == nullptr)
    {
      tex_W = 0;
      tex_H = 0;
      std::cout << "can't open file" << std::endl;
      return {};
    }
    
    unsigned char info[54];
    auto readRes = fread(info, sizeof(unsigned char), 54, f); // read the 54-byte header
    if(readRes != 54)
    {
      std::cout << "can't read 54 byte BMP header" << std::endl;
      return {};
    }
    
    int width  = *(int*)&info[18];
    int height = *(int*)&info[22];
    
    int row_padded = (width*3 + 3) & (~3);
    auto data      = new unsigned char[row_padded];
    
    std::vector<unsigned> res(width*height);
    
    for(int i = 0; i < height; i++)
    {
      fread(data, sizeof(unsigned char), row_padded, f);
      for(int j = 0; j < width; j++)
        res[i*width+j] = (uint32_t(data[j*3+0]) << 16) | (uint32_t(data[j*3+1]) << 8)  | (uint32_t(data[j*3+2]) << 0);
    }
    
    fclose(f);
    delete [] data;
  
    tex_W = unsigned(width);
    tex_H = unsigned(height);
    return res;
  }
  
  static auto texData = LoadBMP("../resources/textures/BlueNoise470.bmp");
  static std::default_random_engine generator;
  static float x_coord = std::uniform_int_distribution<uint>(0, tex_H - 256)(generator);
  static float y_coord = std::uniform_int_distribution<uint>(0, tex_W - 256)(generator);
  
  inline unsigned int get_noise (LiteMath::uint2 pos) {
    uint i = (x_coord + pos.x);
    uint j = (y_coord + pos.y);
    return texData[i * tex_W + j];
  }
  
  void regenerate() {
    x_coord = std::gamma_distribution<float>(0, tex_H - 256)(generator);
    y_coord = std::gamma_distribution<float>(0, tex_W - 256)(generator);
  }
}

#endif //VK_GRAPHICS_BASIC_BLUE_GEN_H
