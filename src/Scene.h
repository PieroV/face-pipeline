/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "glad/glad.h"

#include "glm/glm.hpp"

#include "nlohmann/json.hpp"

#include "open3d/camera/PinholeCameraIntrinsic.h"
#include "open3d/geometry/PointCloud.h"
#include "open3d/geometry/RGBDImage.h"

#include "shaders.h"

class Scene;

class PointCloud {
public:
  PointCloud(const Scene &scene, const std::string &name, double trunc);
  PointCloud(const Scene &scene, const nlohmann::json &j);

  GLsizei getPoints(std::vector<float> &data,
                    const double *voxelSize = nullptr) const;
  glm::mat4 getMatrix() const;
  nlohmann::json toJson() const;
  void loadData(const Scene &scene);

  std::string name;
  glm::vec3 translationPre;
  glm::vec3 euler;
  glm::vec3 translationPost;
  glm::vec3 color;
  glm::mat4 matrix;
  bool rawMatrix;
  bool hidden;
  double trunc;

private:
  std::shared_ptr<open3d::geometry::RGBDImage> mRGBD;
  std::shared_ptr<open3d::geometry::PointCloud> mPointCloud;
};

class Scene {
public:
  static std::pair<std::unique_ptr<Scene>, std::vector<std::string>>
  load(const std::filesystem::path &dataDirectory);
  void save() const;

  void refreshBuffer();
  void render(const glm::mat4 &pv) const;

  const std::filesystem::path &getDataDirectory() const {
    return mDataDirectory;
  }
  const open3d::camera::PinholeCameraIntrinsic &getCameraIntrinsic() {
    return mIntrinsic;
  }
  double getDepthScale() const { return mDepthScale; }

  std::pair<open3d::geometry::Image, open3d::geometry::Image>
  openFrame(const std::filesystem::path &basename) const;

  std::vector<PointCloud> clouds;
  bool paintUniform = false;
  bool voxelDown = false;
  double voxelSize = 0.01;

private:
  struct RenderData {
    GLuint vao = 0;
    GLuint vbo = 0;
    RenderData() = default;
    RenderData(const RenderData &other) = delete;
    RenderData(RenderData &&other) noexcept;
    RenderData &operator=(const RenderData &other) = delete;
    RenderData &operator=(RenderData &&other) noexcept;
    ~RenderData();
  };

  Scene() = default;
  Scene(const std::filesystem::path &dataDirectory,
        std::vector<std::string> &warnings);
  open3d::geometry::Image openImage(const std::string &path) const;
  std::filesystem::path getDataFile() const;

  std::filesystem::path mDataDirectory;

  open3d::camera::PinholeCameraIntrinsic mIntrinsic;
  double mDepthScale;

  // We will create the shader each time we create a Scene instance.
  // This is not optimal, but it is okay to do it for now, maybe we can do
  // something better in the future.
  ShaderProgram mShader;

  std::vector<GLsizei> mNumPoints;

  RenderData mRenderData;

  friend class PointCloud;
};
