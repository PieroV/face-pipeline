/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <random>

#include "glm/glm.hpp"

void initRng();
std::mt19937 &getRng();

glm::vec3 randomColor();
