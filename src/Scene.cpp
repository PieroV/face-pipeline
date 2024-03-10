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
#include <valarray>

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
  std::string rgbPath =
      (mDataDirectory / "rgb" / fs::path(basename).concat(".jpg")).string();
  std::string depthPath =
      (mDataDirectory / "depth" / fs::path(basename).concat(".png")).string();
  auto pair = std::make_pair(openImage(rgbPath), openImage(depthPath));
  // We do not force the number of channels for now.
  if (pair.first.bytes_per_channel_ != 1) {
    throw std::runtime_error("Unsupported format of the RGB image.");
  }
  if (pair.second.num_of_channels_ != 1) {
    throw std::runtime_error("The depth image has more than one channel.");
  }
  return pair;
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
