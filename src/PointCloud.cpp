/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "PointCloud.h"

#include <random>

#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"

#include "open3d/io/ImageIO.h"

#include "Scene.h"
#include "utilities.h"

using json = nlohmann::json;

namespace glm {
void to_json(json &j, const vec3 &v) { j = json{v.x, v.y, v.z}; }

void from_json(const json &j, vec3 &v) {
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
  j.at(2).get_to(v.z);
}

void to_json(json &j, const mat4 &m) {
  j = std::valarray(glm::value_ptr(m), 16);
}

void from_json(const json &j, mat4 &m) {
  std::array<float, 16> data;
  j.get_to(data);
  m = make_mat4(data.data());
}
} // namespace glm

PointCloud::PointCloud(const Scene &scene, const std::string &name,
                       double trunc)
    : name(name), translationPre(0.0f, 0.0f, 0.0f), euler(0.0f, 0.0f, 0.0f),
      translationPost(0.0f, 0.0f, 0.0f), color(randomColor()), matrix(1.0f),
      rawMatrix(false), hidden(false), trunc(trunc) {
  loadData(scene);
}

PointCloud::PointCloud(const Scene &scene, const json &j) {
  j.at("name").get_to(name);
  j.at("translationPre").get_to(translationPre);
  j.at("euler").get_to(euler);
  j.at("translationPost").get_to(translationPost);
  j.at("matrix").get_to(matrix);
  j.at("rawMatrix").get_to(rawMatrix);
  j.at("hidden").get_to(hidden);
  j.at("color").get_to(color);
  j.at("trunc").get_to(trunc);
  loadData(scene);
}

glm::mat4 PointCloud::getMatrix() const {
  if (rawMatrix) {
    return matrix;
  }
  glm::mat4 mtx =
      glm::translate(glm::mat4(1.0f), translationPost) *
      glm::eulerAngleYXZ(glm::radians(euler.y), glm::radians(euler.x),
                         glm::radians(euler.z));
  return glm::translate(mtx, translationPre);
}

Eigen::Matrix4d PointCloud::getMatrixEigen() const {
  return Eigen::Map<const Eigen::Matrix4f>(glm::value_ptr(getMatrix()))
      .cast<double>();
}

void PointCloud::loadData(const Scene &scene) {
  auto [rgb, depth] = scene.openFrame(name);
  mRGBD = open3d::geometry::RGBDImage::CreateFromColorAndDepth(
      rgb, depth, 1.0 / scene.mDepthScale, trunc, false);
  if (!mRGBD) {
    throw std::runtime_error("Failed to create the RGBD image");
  }
  makeMasked(scene);
  mPointCloud = open3d::geometry::PointCloud::CreateFromRGBDImage(
      *mRGBD, scene.mIntrinsic);
  if (!mPointCloud) {
    throw std::runtime_error("Failed to create the point cloud");
  }
}

void PointCloud::makeMasked(const Scene &scene) {
  std::filesystem::path maskPath = scene.getDataDirectory();
  maskPath = maskPath / "mask" / (name + ".png");
  std::error_code ec;
  if (!std::filesystem::exists(maskPath, ec) || ec) {
    return;
  }

  std::string maskFilename = maskPath.string();
  open3d::geometry::Image mask;
  if (!open3d::io::ReadImage(maskFilename, mask)) {
    return;
  }
  if (mask.num_of_channels_ != 4 || mask.width_ != mRGBD->color_.width_ ||
      mask.height_ != mRGBD->color_.height_ || mask.bytes_per_channel_ != 1) {
    fprintf(stderr,
            "%s was opened, but it cannot be used as a mask (wrong size or it "
            "does not have an alpha channel).\n",
            maskFilename.c_str());
    return;
  }

  if (mRGBD->depth_.bytes_per_channel_ != 4) {
    fprintf(stderr, "The depth image %s is not a float. Giving up.\n",
            name.c_str());
    return;
  }
  // Additional sanity checks that should never happen, since they are already
  // verified earlier, so add them as an assertion.
  assert(mRGBD->depth_.num_of_channels_ == 1 &&
         mRGBD->depth_.width_ == mask.width_ &&
         mRGBD->depth_.height_ == mask.height_);
  assert(mask.width_ > 0 && mask.height_ > 0);
  mMaskedRgbd = std::make_shared<open3d::geometry::RGBDImage>(*mRGBD);
  if (!mMaskedRgbd) {
    fprintf(stderr, "%s: will not create the mask RGBD (out of memory).\n",
            name.c_str());
    return;
  }

  size_t pixels = static_cast<size_t>(mask.width_ * mask.height_);
  const uint8_t *maskPtr = mask.data_.data() + 3;
  float *depthPtr = reinterpret_cast<float *>(mMaskedRgbd->depth_.data_.data());
  for (size_t i = 0; i < pixels; i++, maskPtr += 4, depthPtr++) {
    if (*maskPtr < 128) {
      *depthPtr = 0;
    }
  }
}

json PointCloud::toJson() const {
  return {{"name", name},          {"translationPre", translationPre},
          {"euler", euler},        {"translationPost", translationPost},
          {"matrix", getMatrix()}, {"rawMatrix", rawMatrix},
          {"hidden", hidden},      {"color", color},
          {"trunc", trunc}};
}

const open3d::geometry::PointCloud &PointCloud::getPointCloud() const {
  // This should never be nullptr, since we create it on the constructor and
  // throw if creating it failed.
  assert(mPointCloud);
  return *mPointCloud;
}

std::shared_ptr<open3d::geometry::PointCloud>
PointCloud::getPointCloudCopy() const {
  // Should throw std::bad_alloc in case of error.
  auto copy = std::make_shared<open3d::geometry::PointCloud>(getPointCloud());
  assert(copy);
  return copy;
}

const open3d::geometry::RGBDImage &PointCloud::getRgbdImage() const {
  // See getPointCloud.
  assert(mRGBD);
  return *mRGBD;
}

std::shared_ptr<const open3d::geometry::RGBDImage>
PointCloud::getMaskedRgbd() const {
  return std::const_pointer_cast<const open3d::geometry::RGBDImage>(
      mMaskedRgbd);
}
