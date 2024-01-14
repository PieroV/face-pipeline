#pragma once

#include <initializer_list>
#include <memory>

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
  GLint getUniformLocation(const char *name);

  GLuint program;
};

ShaderProgram createShader();
