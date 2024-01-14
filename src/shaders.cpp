#include "shaders.h"

#include <initializer_list>
#include <stdexcept>

static const char vertShader[] = R"THE_SHADER(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform mat4 pvm;
out vec3 pointColor;

void main() {
  gl_Position = pvm * vec4(aPos.xyz, 1.0);
  pointColor = aColor;
}
)THE_SHADER";

static const char fragShader[] = R"THE_SHADER(
#version 330 core
uniform bool paintUniform;
uniform vec3 uniformColor;
in vec3 pointColor;
out vec4 FragColor;

void main()
{
  if (paintUniform) {
    FragColor = vec4(uniformColor, 1.0f);
  } else {
    FragColor = vec4(pointColor, 1.0f);
  }
}
)THE_SHADER";

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

GLint ShaderProgram::getUniformLocation(const char *name) {
  return glGetUniformLocation(program, name);
}

ShaderProgram createShader() {
  Shader vert(GL_VERTEX_SHADER);
  vert.compile(vertShader);
  Shader frag(GL_FRAGMENT_SHADER);
  frag.compile(fragShader);
  ShaderProgram program;
  program.link({vert.shader, frag.shader});
  return program;
}
