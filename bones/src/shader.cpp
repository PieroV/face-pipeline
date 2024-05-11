#include "shader.h"

static const char vertShader[] = R"THE_SHADER(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 3) in ivec4 aIndices;
layout (location = 4) in vec4 aWeights;
uniform mat4 pv;
uniform mat4 bones[50];
uniform bool skinned;
out vec3 pointColor;

void main() {
  if (skinned) {
    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; i++) {
      if (aIndices[i] < 0) {
        break;
      }
      skin += bones[aIndices[i]] * aWeights[i];
    }
    gl_Position = pv * skin * vec4(aPos.xyz, 1.0);
  } else {
    gl_Position = pv * vec4(aPos.xyz, 1.0);
  }
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

ShaderProgram createShader() {
  Shader vert(GL_VERTEX_SHADER);
  vert.compile(vertShader);
  Shader frag(GL_FRAGMENT_SHADER);
  frag.compile(fragShader);
  ShaderProgram program;
  program.link({vert.shader, frag.shader});
  return program;
}
