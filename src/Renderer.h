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

  Renderer();
  Renderer(const Renderer &other) = delete;
  Renderer(Renderer &&other) = delete;
  // We do not implement the move semantics because we have several primitive
  // variables. It is easy to forget some, so we will not do implement until
  // necessary (but we can probably live with a single renderer).
  Renderer &operator=(const Renderer &other) = delete;
  Renderer &operator=(Renderer &&other) = delete;
  ~Renderer();

  void addPointCloud(const PointCloud &pcd,
                     std::optional<double> voxelSize = std::nullopt);
  void addPointCloud(const open3d::geometry::PointCloud &pcd);
  void uploadBuffer();
  void clearBuffer();

  void beginRendering(const glm::mat4 &pv) const;
  void
  renderPointCloud(size_t idx, const glm::mat4 &model,
                   std::optional<glm::vec3> uniformColor = std::nullopt) const;
  void endRendering() const;

  Symmetry mirror = MirrorNone;

private:
  GLuint mVAO = 0;
  GLuint mVBO = 0;

  ShaderProgram mShader;
  GLint mPvLoc;
  GLint mModelLoc;
  GLint mPaintUniformLoc;
  GLint mUniformColorLoc;
  GLint mMirrorLoc;
  GLint mMirrorDrawLoc;

  std::vector<float> mBuffer;
  std::vector<GLsizei> mOffset;
};
