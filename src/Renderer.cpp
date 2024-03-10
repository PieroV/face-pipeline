#include "Renderer.h"

#include <utility>

#include "glm/gtc/type_ptr.hpp"

// clang-format off
static const float axes[] = {
  0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
  1.0, 0.0, 0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
  0.0, 1.0, 0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 0.0, 0.0, 1.0,
  0.0, 0.0, 1.0, 0.0, 0.0, 1.0,
};
// clang-format on

Renderer::Renderer() : mShader(createShader()) {
  mPvLoc = mShader.getUniformLocation("pv");
  mModelLoc = mShader.getUniformLocation("model");
  mPaintUniformLoc = mShader.getUniformLocation("paintUniform");
  mUniformColorLoc = mShader.getUniformLocation("uniformColor");
  mMirrorLoc = mShader.getUniformLocation("mirror");
  mMirrorDrawLoc = mShader.getUniformLocation("mirrorDraw");
  if (mPvLoc < 0 || mModelLoc < 0 || mPaintUniformLoc < 0 ||
      mUniformColorLoc < 0 || mMirrorLoc < 0 || mMirrorDrawLoc < 0) {
    throw std::runtime_error("Could not find a shader uniform.");
  }

  glGenVertexArrays(1, &mVAO);
  glGenBuffers(1, &mVBO);
  glBindVertexArray(mVAO);
  glBindBuffer(GL_ARRAY_BUFFER, mVBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glBindVertexArray(0);

  clearBuffer();
  uploadBuffer();
}

Renderer::~Renderer() {
  glDeleteVertexArrays(1, &mVAO);
  mVAO = 0;
  glDeleteBuffers(1, &mVBO);
  mVBO = 0;
}

void Renderer::addPointCloud(const PointCloud &pcd,
                             std::optional<double> voxelSize) {
  if (voxelSize) {
    auto downSampled = pcd.getPointCloud().VoxelDownSample(*voxelSize);
    if (!downSampled) {
      throw std::runtime_error("Failed to sample the point cloud down");
    }
    addPointCloud(*downSampled);
  } else {
    addPointCloud(pcd.getPointCloud());
  }
}

void Renderer::addPointCloud(const open3d::geometry::PointCloud &pcd) {
  const size_t n = pcd.points_.size();
  // We created a point cloud from an RGBD image, it must have colors.
  assert(pcd.colors_.size() == n);
  // Magic number: 6 floats for each point.
  mBuffer.reserve(mBuffer.size() + n * 6);
  for (size_t i = 0; i < n; i++) {
    const Eigen::Vector3d &pos = pcd.points_[i];
    const Eigen::Vector3d &col = pcd.colors_[i];
    mBuffer.insert(mBuffer.end(), pos.data(), pos.data() + 3);
    mBuffer.insert(mBuffer.end(), col.data(), col.data() + 3);
  }
  mOffset.push_back(static_cast<GLsizei>(mBuffer.size() / 6));
}

void Renderer::uploadBuffer() {
  glBindVertexArray(mVAO);
  glBufferData(GL_ARRAY_BUFFER, mBuffer.size() * sizeof(float), mBuffer.data(),
               GL_STATIC_DRAW);
  glBindVertexArray(0);
}

void Renderer::clearBuffer() {
  mBuffer.assign(axes, axes + sizeof(axes) / sizeof(*axes));
  mOffset.assign({6});
}

void Renderer::beginRendering(const glm::mat4 &pv) const {
  mShader.use();
  glUniformMatrix4fv(mPvLoc, 1, GL_FALSE, glm::value_ptr(pv));
  glBindVertexArray(mVAO);
  glm::mat4 model(1.0f);
  glUniformMatrix4fv(mModelLoc, 1, GL_FALSE, glm::value_ptr(model));
  // Axes are never painted in uniform and never subject to symmetry.
  glUniform1i(mPaintUniformLoc, 0);
  glUniform1i(mMirrorLoc, static_cast<int>(MirrorNone));
  glUniform1i(mMirrorDrawLoc, 0);
  glDrawArrays(GL_LINES, 0, 6);
}

void Renderer::renderPointCloud(size_t idx, const glm::mat4 &model,
                                std::optional<glm::vec3> uniformColor) const {
  glUniformMatrix4fv(mModelLoc, 1, GL_FALSE, glm::value_ptr(model));
  glUniform1i(mPaintUniformLoc, uniformColor ? 1 : 0);
  if (uniformColor) {
    glUniform3fv(mUniformColorLoc, 1, glm::value_ptr(*uniformColor));
  }
  GLsizei offset = mOffset.at(idx);
  GLsizei num = mOffset.at(idx + 1) - offset;
  glUniform1i(mMirrorLoc, static_cast<int>(mirror));
  glDrawArrays(GL_POINTS, offset, num);
  if (mirror != MirrorNone) {
    // Draw again, the shader will reverse the positions.
    glUniform1i(mMirrorDrawLoc, 1);
    glDrawArrays(GL_POINTS, offset, num);
    glUniform1i(mMirrorDrawLoc, 0);
  }
}

void Renderer::endRendering() const { glBindVertexArray(0); }
