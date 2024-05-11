/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "shaders.h"

#include <initializer_list>
#include <stdexcept>

static const char vertShader[] = R"THE_SHADER(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexPos;
uniform mat4 pv;
uniform mat4 model;
uniform int mirror;
uniform bool mirrorDraw;
out vec3 pointColor;
out vec2 texPos;

#define MirrorNone 0
#define MirrorOnNegX 1
#define MirrorOnPosX 2

void main() {
  bool shouldDiscard = false;
  vec4 pos = model * vec4(aPos.xyz, 1.0);
  if (mirrorDraw && pos.x == 0.0) {
    // Draw stuff on the axis once.
    // For now, if mirrorDraw is true, we have symmetry only on the X axis.
    // We might have to change this in the future if we implement other axes.
    shouldDiscard = true;
  } if (mirror == MirrorOnNegX) {
    if (pos.x < 0) {
      shouldDiscard = true;
    } else if (mirrorDraw) {
      pos.x = -pos.x;
    }
  } else if (mirror == MirrorOnPosX) {
    if (pos.x > 0) {
      shouldDiscard = true;
    } else if (mirrorDraw) {
      pos.x = -pos.x;
    }
  }
  // Discarding isn't really a thing, so abuse the clipping space :)
  if (shouldDiscard) {
    gl_Position = vec4(-2.0, 0, 0, 1.0);
  } else {
    gl_Position = pv * pos;
  }

  pointColor = aColor;
  texPos = aTexPos;
}
)THE_SHADER";

static const char fragShader[] = R"THE_SHADER(
#version 330 core
uniform bool paintUniform;
uniform vec3 uniformColor;
uniform bool useTexture;
uniform sampler2D theTexture;
in vec3 pointColor;
in vec2 texPos;
out vec4 FragColor;

void main()
{
  if (paintUniform) {
    FragColor = vec4(uniformColor, 1.0f);
  } else if (useTexture) {
    FragColor = texture(theTexture, texPos);
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
