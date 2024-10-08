/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "Scene.h"

#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>

#include "nlohmann/json.hpp"

#include "open3d/io/ImageIO.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

Scene::Scene(const std::filesystem::path &dataDirectory,
             std::vector<std::string> &warnings)
    : mDataDirectory(dataDirectory) {
  if (!fs::exists(dataDirectory) || !fs::is_directory(dataDirectory)) {
    throw std::invalid_argument(dataDirectory.string() +
                                " does not exist or is not a directory.");
  }

  fs::path cameraConfig = dataDirectory / "camera.json";
  if (!fs::exists(cameraConfig)) {
    throw std::invalid_argument(dataDirectory.string() + " does not exist.");
  }
  {
    int width, height;
    double fx, fy, ppx, ppy;
    std::ifstream cameraStream(cameraConfig);
    json camera;
    cameraStream >> camera;
    camera.at("width").get_to(width);
    camera.at("height").get_to(height);
    if (width <= 0 || height <= 0) {
      throw std::domain_error("Invalid width or height in camera.json.");
    }
    camera.at("fx").get_to(fx);
    camera.at("fy").get_to(fy);
    camera.at("ppx").get_to(ppx);
    camera.at("ppy").get_to(ppy);
    mIntrinsic.SetIntrinsics(width, height, fx, fy, ppx, ppy);
    camera.at("scale").get_to(mDepthScale);
  }

  fs::path dataPath = getDataFile();
  if (fs::exists(dataPath)) {
    std::ifstream data(dataPath);
    json j;
    data >> j;
    for (const auto &p : j) {
      try {
        clouds.emplace_back(*this, p);
      } catch (std::exception &e) {
        warnings.push_back(e.what());
      }
    }
  }
}

open3d::geometry::Image Scene::openImage(const std::string &path) const {
  using namespace open3d;
  geometry::Image img;
  if (!io::ReadImage(path, img)) {
    throw std::runtime_error("Cannot open " + path + ".");
  }
  if (img.width_ != mIntrinsic.width_ || img.height_ != mIntrinsic.height_) {
    char error[100];
    snprintf(error, sizeof(error), " has a wrong size (%dx%d, expected %dx%d).",
             img.width_, img.height_, mIntrinsic.width_, mIntrinsic.height_);
    throw std::runtime_error(path + error);
  }
  return img;
}

std::pair<open3d::geometry::Image, open3d::geometry::Image>
Scene::openFrame(const std::filesystem::path &basename) const {
  std::string rgb = ("rgb" / fs::path(basename).concat(".jpg")).string();
  std::string depth = ("depth" / fs::path(basename).concat(".png")).string();
  return openFrame(rgb, depth);
}

std::pair<open3d::geometry::Image, open3d::geometry::Image>
Scene::openFrame(std::string rgb, std::string depth) const {
  std::string prefix =
      mDataDirectory.string() + std::filesystem::path::preferred_separator;
  rgb = prefix + rgb;
  depth = prefix + depth;
  auto pair = std::make_pair(openImage(rgb), openImage(depth));
  // We do not force the number of channels for now.
  if (pair.first.bytes_per_channel_ != 1) {
    throw std::runtime_error("Unsupported format of the RGB image.");
  }
  if (pair.second.num_of_channels_ != 1) {
    throw std::runtime_error("The depth image has more than one channel.");
  }
  return pair;
}

std::pair<std::vector<Eigen::Vector3d>,
          std::vector<Eigen::Vector2<unsigned int>>>
Scene::unprojectDepth(const open3d::geometry::Image &depth,
                      const Eigen::Matrix4d &transform) const {
  if (depth.width_ <= 0 || depth.height_ <= 0) {
    throw std::invalid_argument("The depth image cannot be empty.");
  }
  if (depth.bytes_per_channel_ != 4 || depth.num_of_channels_ != 1) {
    throw std::invalid_argument(
        "The depth image should be a float32 single-channel image.");
  }
  std::vector<Eigen::Vector3d> points;
  std::vector<Eigen::Vector2<unsigned int>> pixels;
  double principal[] = {mIntrinsic.GetPrincipalPoint().first,
                        mIntrinsic.GetPrincipalPoint().second};
  double focal[] = {mIntrinsic.GetFocalLength().first,
                    mIntrinsic.GetFocalLength().second};
  unsigned int width = static_cast<unsigned int>(depth.width_);
  unsigned int height = static_cast<unsigned int>(depth.height_);
  const float *z = reinterpret_cast<const float *>(depth.data_.data());
  for (unsigned int y = 0; y < height; y++) {
    for (unsigned int x = 0; x < width; x++, z++) {
      if (*z <= 0) {
        continue;
      }
      Eigen::Vector4d point((x - principal[0]) * *z / focal[0],
                            (y - principal[1]) * *z / focal[1], *z, 1.0);
      point = transform * point;
      assert(fabs(point[3] - 1.0) < 1e-5);
      points.emplace_back(point[0], point[1], point[2]);
      pixels.emplace_back(x, y);
    }
  }
  return {std::move(points), std::move(pixels)};
}

std::pair<std::vector<Eigen::Vector3d>,
          std::vector<Eigen::Vector2<unsigned int>>>
Scene::unprojectDepth(const PointCloud &pcd, bool useMask) const {
  const open3d::geometry::RGBDImage &rgbd = useMask && pcd.hasMaskedRgbd()
                                                ? *pcd.getMaskedRgbd()
                                                : pcd.getRgbdImage();
  return unprojectDepth(rgbd.depth_, pcd.getMatrixEigen());
}

std::pair<std::unique_ptr<Scene>, std::vector<std::string>>
Scene::load(const fs::path &dataDirectory) {
  std::vector<std::string> warnings;
  // no make_unique, as the constructor is private.
  std::unique_ptr<Scene> self(new Scene(dataDirectory, warnings));
  return {std::move(self), warnings};
}

fs::path Scene::getDataFile() const {
  return mDataDirectory / "face-pipeline.json";
}

void Scene::save() const {
  std::ofstream o(getDataFile());
  json j;
  for (const PointCloud &pcd : clouds) {
    j.push_back(pcd.toJson());
  }
  o << std::setw(2) << j;
}
