/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "GLObjects.h"

GLObjects::GLObjects() {
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);
}

GLObjects::~GLObjects() {
  glDeleteVertexArrays(1, &vao);
  vao = 0;
  glDeleteBuffers(1, &vbo);
  vbo = 0;
  glDeleteBuffers(1, &ebo);
  ebo = 0;
}
