/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "utilities.h"

std::mt19937 rng;

void initRng() {
  std::random_device rd;
  rng.seed(rd());
}

std::mt19937 &getRng() { return rng; }

glm::vec3 randomColor() {
  std::uniform_int_distribution<int> colorDist(0, 2);
  int color = colorDist(rng);
  std::uniform_real_distribution<float> hmod2Dist(0.0f, 2.0f);
  float hmod2 = hmod2Dist(rng);
  std::uniform_real_distribution<float> sDist(0.6f, 1.0f);
  float s = sDist(rng);
  std::uniform_real_distribution<float> vDist(0.8f, 1.0f);
  float v = vDist(rng);

  float c = v * s;
  float x = c * (1 - fabs(hmod2 - 1));
  if (hmod2 >= 1.0f) {
    std::swap(c, x);
  }

  glm::vec3 rgb(0, 0, 0);
  switch (color) {
  case 0:
    rgb = glm::vec3(c, x, 0.0f);
    break;
  case 1:
    rgb = glm::vec3(0.0f, c, x);
    break;
  case 2:
    rgb = glm::vec3(x, 0.0f, c);
    break;
  default:
    assert(false && "Should never be here");
  }

  return rgb + glm::vec3(v - c);
}
