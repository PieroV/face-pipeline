/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include "glad/glad.h"

struct GLObjects {
  GLObjects();
  GLObjects(const GLObjects &) = delete;
  GLObjects(GLObjects &&) = delete;
  GLObjects &operator=(const GLObjects &) = delete;
  GLObjects &operator=(GLObjects &&) = delete;
  ~GLObjects();
  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ebo = 0;
};
