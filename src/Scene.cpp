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

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"

#include "open3d/io/ImageIO.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

std::mt19937 rng;

// clang-format off
static const float axes[] = {
  0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
  1.0, 0.0, 0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
  0.0, 1.0, 0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 0.0, 0.0, 1.0,
  0.0, 0.0, 1.0, 0.0, 0.0, 1.0,
};
// clang-format on

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

PointCloud::PointCloud(const Scene &scene, const std::string &name)
    : name(name), translationPre(0.0f, 0.0f, 0.0f), euler(0.0f, 0.0f, 0.0f),
      translationPost(0.0f, 0.0f, 0.0f), matrix(1.0f), rawMatrix(false),
      hidden(false) {
  std::uniform_real_distribution<float> d(0.0, 1.0);
  color.x = d(rng);
  color.y = d(rng);
  color.z = d(rng);
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

GLsizei PointCloud::getPoints(std::vector<float> &data,
                              const double *voxelSize) const {
  assert(mPointCloud);
  std::shared_ptr<open3d::geometry::PointCloud> pcd =
      voxelSize ? mPointCloud->VoxelDownSample(*voxelSize) : mPointCloud;
  if (!pcd) {
    throw std::runtime_error("Failed to sample the point cloud down");
  }
  const size_t n = pcd->points_.size();
  // We created a point cloud from an RGBD image, it must have colors.
  assert(pcd->colors_.size() == n);
  data.reserve(data.size() + n * 6);
  for (size_t i = 0; i < n; i++) {
    Eigen::Vector3d &pos = pcd->points_[i];
    Eigen::Vector3d &col = pcd->colors_[i];
    data.insert(data.end(), pos.data(), pos.data() + 3);
    data.insert(data.end(), col.data(), col.data() + 3);
  }
  return static_cast<GLsizei>(n);
}

void PointCloud::loadData(const Scene &scene) {
  const fs::path &dir = scene.mDataDirectory;
  std::string rgbPath = (dir / "rgb" / (name + ".jpg")).string();
  std::string depthPath = (dir / "depth" / (name + ".png")).string();
  open3d::geometry::Image rgb, depth;
  if (!open3d::io::ReadImageFromJPG(rgbPath, rgb)) {
    throw std::runtime_error("Could not read the RGB frame" + rgbPath);
  }
  if (!open3d::io::ReadImage(depthPath, depth)) {
    throw std::runtime_error("Could not read the depth frame" + depthPath);
  }
  mRGBD = open3d::geometry::RGBDImage::CreateFromColorAndDepth(
      rgb, depth, 1.0 / scene.mDepthScale, trunc, false);
  if (!mRGBD) {
    throw std::runtime_error("Failed to load the RGBD image");
  }
  mPointCloud = open3d::geometry::PointCloud::CreateFromRGBDImage(
      *mRGBD, scene.mIntrinsic);
  if (!mPointCloud) {
    throw std::runtime_error("Failed ot create the point cloud");
  }
  mPointCloud->EstimateNormals(open3d::geometry::KDTreeSearchParamKNN(100));
}

json PointCloud::toJson() const {
  return {{"name", name},          {"translationPre", translationPre},
          {"euler", euler},        {"translationPost", translationPost},
          {"matrix", getMatrix()}, {"rawMatrix", rawMatrix},
          {"hidden", hidden},      {"color", color},
          {"trunc", trunc}};
}

Scene::Scene(const std::filesystem::path &dataDirectory)
    : mDataDirectory(dataDirectory), mShader(createShader()) {
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
      clouds.emplace_back(*this, p);
    }
  }

  {
    std::random_device rd;
    rng.seed(rd());
  }

  {
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    refreshBuffer();
  }

  mPvmLoc = mShader.getUniformLocation("pvm");
  mPaintUniformLoc = mShader.getUniformLocation("paintUniform");
  mUniformColorLoc = mShader.getUniformLocation("uniformColor");
}

Scene::Scene(Scene &&other) noexcept : Scene() { *this = std::move(other); }

Scene &Scene::operator=(Scene &&other) noexcept {
  std::swap(clouds, other.clouds);
  std::swap(mVAO, other.mVAO);
  std::swap(mVBO, other.mVBO);
  std::swap(mShader, other.mShader);
  std::swap(mPvmLoc, other.mPvmLoc);
  std::swap(mPaintUniformLoc, other.mPaintUniformLoc);
  std::swap(mUniformColorLoc, other.mUniformColorLoc);
  return *this;
}

Scene::~Scene() {
  glDeleteVertexArrays(1, &mVAO);
  mVAO = 0;
  glDeleteBuffers(1, &mVBO);
  mVBO = 0;
  mPvmLoc = -1;
  mPaintUniformLoc = -1;
  mUniformColorLoc = -1;
}

std::vector<std::string> Scene::loadFrames() { return {}; }

std::pair<std::unique_ptr<Scene>, std::vector<std::string>>
Scene::load(const std::filesystem::path &dataDirectory) {
  // no make_unique, as the constructor is private.
  std::unique_ptr<Scene> self(new Scene(dataDirectory));
  auto warnings = self->loadFrames();
  return {std::move(self), warnings};
}

std::filesystem::path Scene::getDataFile() const {
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

void Scene::refreshBuffer() {
  std::vector<float> buffer;
  buffer.assign(axes, axes + sizeof(axes) / sizeof(*axes));
  mNumPoints.clear();
  mNumPoints.reserve(clouds.size());
  for (const auto &cloud : clouds) {
    mNumPoints.push_back(
        cloud.getPoints(buffer, voxelDown ? &voxelSize : nullptr));
  }
  glBindVertexArray(mVAO);
  glBufferData(GL_ARRAY_BUFFER, buffer.size(), buffer.data(), GL_STATIC_DRAW);
  glBindVertexArray(0);
}

void Scene::render(const glm::mat4 &pv) const {
  assert(mNumPoints.size() == clouds.size());
  mShader.use();
  glBindVertexArray(mVAO);

  glUniformMatrix4fv(mPvmLoc, 1, GL_FALSE, glm::value_ptr(pv));
  glUniform1i(mPaintUniformLoc, 0);
  glDrawArrays(GL_LINES, 0, 6);
  glUniform1i(mPaintUniformLoc, paintUniform);

  GLsizei offset = 6;
  for (size_t i = 0; i < clouds.size(); i++) {
    const PointCloud &pcd = clouds[i];
    if (!pcd.hidden) {
      glm::mat4 pvm = pv * pcd.getMatrix();
      glUniformMatrix4fv(mPvmLoc, 1, GL_FALSE, glm::value_ptr(pvm));
      glUniform3fv(mUniformColorLoc, 1, glm::value_ptr(pcd.color));
      glDrawArrays(GL_POINTS, offset, mNumPoints[i]);
    }
    offset += mNumPoints[i];
  }
  glBindVertexArray(0);
}
