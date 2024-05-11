/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "ShaderProgram.h"

#include <stdexcept>
#include <string>
#include <utility>

Shader::Shader(GLenum type) noexcept { shader = glCreateShader(type); }

Shader::Shader(Shader &&other) noexcept {
  shader = other.shader;
  other.shader = 0;
}

Shader &Shader::operator=(Shader &&other) noexcept {
  std::swap(shader, other.shader);
  return *this;
}

Shader::~Shader() {
  glDeleteShader(shader);
  shader = 0;
}

void Shader::compile(const char *source) {
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  GLint success = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
    std::string error = "Cannot compile the shader: ";
    error += infoLog;
    glDeleteShader(shader);
    throw std::runtime_error(error);
  }
}

ShaderProgram::ShaderProgram() noexcept { program = glCreateProgram(); }

ShaderProgram::ShaderProgram(ShaderProgram &&other) noexcept {
  program = other.program;
  other.program = 0;
}

ShaderProgram &ShaderProgram::operator=(ShaderProgram &&other) noexcept {
  std::swap(program, other.program);
  return *this;
}

ShaderProgram::~ShaderProgram() {
  glDeleteProgram(program);
  program = 0;
}

void ShaderProgram::link(const std::initializer_list<GLuint> &shaders) {
  for (GLuint shader : shaders) {
    glAttachShader(program, shader);
  }
  glLinkProgram(program);

  GLint success = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
    std::string error = "Cannot link the shader: ";
    error += infoLog;
    glDeleteShader(program);
    throw std::runtime_error(error);
  }
}

void ShaderProgram::use() const { glUseProgram(program); }

GLint ShaderProgram::getUniformLocation(const char *name) const {
  return glGetUniformLocation(program, name);
}

void ShaderProgram::getUniformLocations(const char *names[], GLint *locations,
                                        size_t num) {
  while (num-- > 0) {
    *locations = glGetUniformLocation(program, *names++);
    if (*locations < 0) {
      constexpr size_t len = 199;
      char message[len + 1];
      snprintf(message, len, "Cannot find the %s uniform.", *names);
      message[len] = 0;
      throw std::invalid_argument(message);
    }
    locations++;
  }
}
