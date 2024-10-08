/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <optional>

#include "glm/glm.hpp"

#include "nlohmann/json.hpp"

#include "open3d/geometry/PointCloud.h"
#include "open3d/geometry/RGBDImage.h"

class Scene;

class PointCloud {
public:
  PointCloud(const Scene &scene, const std::string &name, double trunc);
  PointCloud(const Scene &scene, const std::string &name,
             const std::string &rgb, const std::string depth, double trunc);
  PointCloud(const Scene &scene, const nlohmann::json &j);

  Eigen::Matrix4d getMatrixEigen() const;
  nlohmann::json toJson() const;
  void loadData(const Scene &scene);

  const open3d::geometry::PointCloud &getPointCloud() const;
  std::shared_ptr<open3d::geometry::PointCloud> getPointCloudCopy() const;
  const open3d::geometry::RGBDImage &getRgbdImage() const;
  std::shared_ptr<const open3d::geometry::RGBDImage> getMaskedRgbd() const;
  bool hasMaskedRgbd() const { return static_cast<bool>(mMaskedRgbd); }
  const open3d::geometry::PointCloud &
  getMaskedPointCloud(bool allowFallback = true) const;

  std::string name;
  std::string rgb;
  std::string depth;
  glm::vec3 color;
  glm::mat4 matrix;
  bool hidden;
  double trunc;

private:
  void makeMasked(const Scene &scene);

  // Open3D uses shared_ptrs, but we throw when we create them they are nullptr.
  // So, they will never be nullptr and you can dereference them without further
  // checks.
  std::shared_ptr<open3d::geometry::RGBDImage> mRGBD;
  std::shared_ptr<open3d::geometry::PointCloud> mPointCloud;
  std::shared_ptr<open3d::geometry::RGBDImage> mMaskedRgbd;
  std::shared_ptr<open3d::geometry::PointCloud> mMaskedCloud;
};
