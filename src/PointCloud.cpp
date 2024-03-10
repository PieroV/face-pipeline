/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "PointCloud.h"

#include <random>

#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"

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

void PointCloud::loadData(const Scene &scene) {
  auto [rgb, depth] = scene.openFrame(name);
  mRGBD = open3d::geometry::RGBDImage::CreateFromColorAndDepth(
      rgb, depth, 1.0 / scene.mDepthScale, trunc, false);
  if (!mRGBD) {
    throw std::runtime_error("Failed to create the RGBD image");
  }
  mPointCloud = open3d::geometry::PointCloud::CreateFromRGBDImage(
      *mRGBD, scene.mIntrinsic);
  if (!mPointCloud) {
    throw std::runtime_error("Failed to create the point cloud");
  }
}

json PointCloud::toJson() const {
  return {{"name", name},          {"translationPre", translationPre},
          {"euler", euler},        {"translationPost", translationPost},
          {"matrix", getMatrix()}, {"rawMatrix", rawMatrix},
          {"hidden", hidden},      {"color", color},
          {"trunc", trunc}};
}
