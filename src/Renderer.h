/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <optional>
#include <vector>

#include "glad/glad.h"

#include "glm/glm.hpp"

#include "open3d/geometry/PointCloud.h"

#include "PointCloud.h"
#include "shaders.h"

class Renderer {
public:
  enum Symmetry {
    MirrorNone,
    MirrorOnNegX,
    MirrorOnPosX,
    MirrorMax,
  };

  enum VertexAttribute {
    VA_X,
    VA_Y,
    VA_Z,
    VA_R,
    VA_G,
    VA_B,
    VA_U,
    VA_V,
    VA_MAX,
  };

  using VertexMatrix =
      Eigen::Matrix<float, Eigen::Dynamic, VA_MAX, Eigen::RowMajor>;
  using Vertex = Eigen::Vector<float, VA_MAX>;

  Renderer();
  Renderer(const Renderer &other) = delete;
  Renderer(Renderer &&other) = delete;
  // We do not implement the move semantics because we have several primitive
  // variables. It is easy to forget some, so we will not do implement until
  // necessary (but we can probably live with a single renderer).
  Renderer &operator=(const Renderer &other) = delete;
  Renderer &operator=(Renderer &&other) = delete;
  ~Renderer();

  size_t addPointCloud(const PointCloud &pcd,
                       std::optional<double> voxelSize = std::nullopt);
  size_t addPointCloud(const open3d::geometry::PointCloud &pcd);
  size_t addTriangleMesh(const open3d::geometry::TriangleMesh &mesh);
  size_t addTriangleMesh(const VertexMatrix &vertices,
                         const std::vector<uint32_t> &indices);
  void uploadBuffer() const;
  void clearBuffer();

  void beginRendering(const glm::mat4 &pv) const;
  void
  renderPointCloud(size_t idx, const glm::mat4 &model = glm::mat4(1.0f),
                   std::optional<glm::vec3> uniformColor = std::nullopt) const;
  void
  renderIndexedMesh(size_t idx, const glm::mat4 &model = glm::mat4(1.0f),
                    bool textured = false, GLsizei offset = 0,
                    GLsizei count = std::numeric_limits<GLsizei>::max()) const;
  void endRendering() const;

  Symmetry mirror = MirrorNone;

private:
  enum Uniforms : unsigned {
    U_PV,
    U_Model,
    U_Mirror,
    U_MirrorDraw,
    U_PaintUniform,
    U_UniformColor,
    U_UseTexture,
    U_Texture,
    U_Max,
  };
  void addVertices(const VertexMatrix &vertices);
  void addPoints(const std::vector<Eigen::Vector3d> &points,
                 const std::vector<Eigen::Vector3d> &colors);

  GLuint mVAO = 0;
  GLuint mVBO = 0;
  GLuint mEBO = 0;

  ShaderProgram mShader;
  GLint mUniforms[U_Max];

  VertexMatrix mBuffer;
  std::vector<GLsizei> mOffsets;
  std::vector<uint32_t> mIndices;
  std::vector<GLsizei> mIndexOffsets;
};
