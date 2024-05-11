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

#include "open3d/camera/PinholeCameraIntrinsic.h"

#include "PointCloud.h"
#include "shaders.h"

class Scene {
public:
  static std::pair<std::unique_ptr<Scene>, std::vector<std::string>>
  load(const std::filesystem::path &dataDirectory);
  void save() const;

  const std::filesystem::path &getDataDirectory() const {
    return mDataDirectory;
  }
  const open3d::camera::PinholeCameraIntrinsic &getCameraIntrinsic() const {
    return mIntrinsic;
  }
  double getDepthScale() const { return mDepthScale; }

  std::pair<open3d::geometry::Image, open3d::geometry::Image>
  openFrame(const std::filesystem::path &basename) const;

  std::pair<std::vector<Eigen::Vector3d>,
            std::vector<Eigen::Vector2<unsigned int>>>
  unprojectDepth(
      const open3d::geometry::Image &depth,
      const Eigen::Matrix4d &transform = Eigen::Matrix4d::Identity()) const;
  std::pair<std::vector<Eigen::Vector3d>,
            std::vector<Eigen::Vector2<unsigned int>>>
  unprojectDepth(const PointCloud &pcd, bool useMask = true) const;

  std::vector<PointCloud> clouds;

private:
  Scene() = default;
  Scene(const std::filesystem::path &dataDirectory,
        std::vector<std::string> &warnings);
  open3d::geometry::Image openImage(const std::string &path) const;
  std::filesystem::path getDataFile() const;

  std::filesystem::path mDataDirectory;

  open3d::camera::PinholeCameraIntrinsic mIntrinsic;
  double mDepthScale;
};
