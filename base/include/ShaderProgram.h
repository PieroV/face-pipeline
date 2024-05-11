/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <initializer_list>

#include <cstddef>

#include "glad/glad.h"

struct Shader {
  Shader(GLenum type) noexcept;
  Shader(const Shader &other) = delete;
  Shader(Shader &&other) noexcept;
  Shader &operator=(const Shader &other) = delete;
  Shader &operator=(Shader &&other) noexcept;
  ~Shader();
  void compile(const char *source);

  GLuint shader;
};

struct ShaderProgram {
  ShaderProgram() noexcept;
  ShaderProgram(const ShaderProgram &other) = delete;
  ShaderProgram(ShaderProgram &&other) noexcept;
  ShaderProgram &operator=(const ShaderProgram &other) = delete;
  ShaderProgram &operator=(ShaderProgram &&other) noexcept;
  ~ShaderProgram();

  void link(const std::initializer_list<GLuint> &shaders);

  void use() const;
  GLint getUniformLocation(const char *name) const;
  void getUniformLocations(const char *names[], GLint *locations, size_t num);

  GLuint program;
};
