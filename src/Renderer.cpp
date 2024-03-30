/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "Renderer.h"

#include <algorithm>
#include <utility>

#include "glm/gtc/type_ptr.hpp"

#include "open3d/geometry/TriangleMesh.h"

// clang-format off
static const Eigen::Matrix<float, 6, 8, Eigen::RowMajor> axes {
  {0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  {1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0},
  {0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
  {0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0},
  {0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0},
  {0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0},
};
// clang-format on

Renderer::Renderer() : mShader(createShader()) {
  static const char *uniformNames[U_Max] = {
      "pv",           "model",        "mirror",     "mirrorDraw",
      "paintUniform", "uniformColor", "useTexture", "theTexture"};
  for (size_t i = 0; i < U_Max; i++) {
    mUniforms[i] = mShader.getUniformLocation(uniformNames[i]);
    if (mUniforms[i] < 0) {
      constexpr size_t len = 99;
      char message[len + 1];
      snprintf(message, len, "Cannot find the %s uniform.", uniformNames[i]);
      message[len] = 0;
      throw std::runtime_error(message);
    }
  }

  glGenVertexArrays(1, &mVAO);
  glGenBuffers(1, &mVBO);
  glBindVertexArray(mVAO);
  glBindBuffer(GL_ARRAY_BUFFER, mVBO);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                        mBuffer.cols() * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                        mBuffer.cols() * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                        mBuffer.cols() * sizeof(float),
                        (void *)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glGenBuffers(1, &mEBO);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
  glBindVertexArray(0);

  clearBuffer();
  uploadBuffer();
}

Renderer::~Renderer() {
  glDeleteVertexArrays(1, &mVAO);
  mVAO = 0;
  glDeleteBuffers(1, &mVBO);
  mVBO = 0;
  glDeleteBuffers(1, &mEBO);
  mEBO = 0;
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

void Renderer::addVertices(const VertexMatrix &vertices) {
  Eigen::Index n = vertices.rows();
  mBuffer.conservativeResize(mBuffer.rows() + n, Eigen::NoChange);
  mBuffer.block(mBuffer.rows() - n, 0, n, mBuffer.cols()) = vertices;
  mOffsets.push_back(static_cast<GLsizei>(mBuffer.rows()));
}

void Renderer::addPoints(const std::vector<Eigen::Vector3d> &points,
                         const std::vector<Eigen::Vector3d> &colors) {
  using namespace Eigen;
  const size_t n = points.size();
  // The caller should have already checked this.
  assert(colors.size() == n);
  // We access the first element.
  if (!n) {
    // Be coherent, and add the offset anyway.
    mOffsets.push_back(static_cast<GLsizei>(mBuffer.rows()));
    return;
  }

  static_assert(sizeof(Vector3d) == 3 * sizeof(double),
                "Cannot map Vector3d to 3 double");
  Map<const Matrix<double, Dynamic, 3, RowMajor>> pos(points[0].data(), n, 3);
  Map<const Matrix<double, Dynamic, 3, RowMajor>> col(colors[0].data(), n, 3);
  Matrix<double, Dynamic, VertexMatrix::ColsAtCompileTime, RowMajor> vertices(
      n, mBuffer.cols());
  vertices << pos, col, MatrixXd::Zero(n, mBuffer.cols() - 6);
  addVertices(vertices.cast<float>());
}

void Renderer::addPointCloud(const open3d::geometry::PointCloud &pcd) {
  addPoints(pcd.points_, pcd.colors_);
  mIndexOffsets.push_back(static_cast<GLsizei>(mIndices.size()));
}

void Renderer::addTriangleMesh(const open3d::geometry::TriangleMesh &mesh) {
  if (mesh.triangles_.empty()) {
    // We always add a new entry in the offsets.
    mOffsets.push_back(static_cast<GLsizei>(mBuffer.rows()));
    mIndexOffsets.push_back(static_cast<GLsizei>(mIndices.size()));
    return;
  }

  if (mesh.vertex_colors_.size() != mesh.vertices_.size()) {
    throw std::runtime_error(
        "Only colored meshes are supported at the moment.");
  }
  addPoints(mesh.vertices_, mesh.vertex_colors_);

  static_assert(sizeof(Eigen::Vector3i) == 3 * sizeof(int),
                "Cannot copy indices as they were raw data.");
  const int *triangles = mesh.triangles_[0].data();
  mIndices.insert(mIndices.end(), triangles,
                  triangles + mesh.triangles_.size() * 3);
  mIndexOffsets.push_back(static_cast<GLsizei>(mIndices.size()));
}

void Renderer::addTriangleMesh(const VertexMatrix &vertices,
                               const std::vector<uint32_t> &indices) {
  addVertices(vertices);
  // Should we use C++20's ranges instead?
  mIndices.insert(mIndices.end(), indices.begin(), indices.end());
  mIndexOffsets.push_back(static_cast<GLsizei>(mIndices.size()));
}

void Renderer::uploadBuffer() const {
  assert(mOffsets.size() == mIndexOffsets.size());
  glBindVertexArray(mVAO);
  glBufferData(GL_ARRAY_BUFFER, mBuffer.size() * sizeof(float), mBuffer.data(),
               GL_STATIC_DRAW);
  if (!mIndices.empty()) {
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mIndices.size() * sizeof(int),
                 mIndices.data(), GL_STATIC_DRAW);
  }
  glBindVertexArray(0);
}

void Renderer::clearBuffer() {
  mBuffer = axes;
  mOffsets.assign({static_cast<GLsizei>(axes.rows())});
  mIndices.clear();
  // No indices used by the axes
  mIndexOffsets.assign({0});
}

void Renderer::beginRendering(const glm::mat4 &pv) const {
  mShader.use();
  glUniformMatrix4fv(mUniforms[U_PV], 1, GL_FALSE, glm::value_ptr(pv));
  glBindVertexArray(mVAO);
  glm::mat4 model(1.0f);
  glUniformMatrix4fv(mUniforms[U_Model], 1, GL_FALSE, glm::value_ptr(model));
  // Axes are never painted in uniform and never subject to symmetry.
  glUniform1i(mUniforms[U_Mirror], static_cast<int>(MirrorNone));
  glUniform1i(mUniforms[U_MirrorDraw], 0);
  glUniform1i(mUniforms[U_PaintUniform], 0);
  glUniform1i(mUniforms[U_UseTexture], 0);
  glDrawArrays(GL_LINES, 0, axes.rows());
}

void Renderer::renderPointCloud(size_t idx, const glm::mat4 &model,
                                std::optional<glm::vec3> uniformColor) const {
  glUniformMatrix4fv(mUniforms[U_Model], 1, GL_FALSE, glm::value_ptr(model));
  glUniform1i(mUniforms[U_PaintUniform], uniformColor ? 1 : 0);
  if (uniformColor) {
    glUniform3fv(mUniforms[U_UniformColor], 1, glm::value_ptr(*uniformColor));
  }
  // 0 are axes, so idx is actually idx + 1 here.
  GLsizei offset = mOffsets.at(idx);
  GLsizei count = mOffsets.at(idx + 1) - offset;
  glUniform1i(mUniforms[U_Mirror], static_cast<int>(mirror));
  glDrawArrays(GL_POINTS, offset, count);
  if (mirror != MirrorNone) {
    // Draw again, the shader will reverse the positions.
    glUniform1i(mUniforms[U_MirrorDraw], 1);
    glDrawArrays(GL_POINTS, offset, count);
    glUniform1i(mUniforms[U_MirrorDraw], 0);
  }
}

void Renderer::renderIndexedMesh(size_t idx, const glm::mat4 &model,
                                 bool textured, GLsizei offset,
                                 GLsizei count) const {
  glUniformMatrix4fv(mUniforms[U_Model], 1, GL_FALSE, glm::value_ptr(model));
  glUniform1i(mUniforms[U_PaintUniform], 0);
  glUniform1i(mUniforms[U_Mirror], 0);
  glUniform1i(mUniforms[U_MirrorDraw], 0);
  glUniform1i(mUniforms[U_UseTexture], textured ? 1 : 0);
  glUniform1i(mUniforms[U_Texture], 0);
  GLsizei start = mIndexOffsets.at(idx);
  GLsizei maxCount = mIndexOffsets.at(idx + 1) - start;
  offset = std::min(offset, maxCount);
  count = std::min(count, maxCount - offset);
  glDrawElementsBaseVertex(
      GL_TRIANGLES, count, GL_UNSIGNED_INT,
      (void *)(uintptr_t)((start + offset) * sizeof(uint32_t)),
      static_cast<GLint>(mOffsets.at(idx)));
}

void Renderer::endRendering() const { glBindVertexArray(0); }
