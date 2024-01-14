#include "Scene.h"

#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <valarray>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"

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

PointCloud::PointCloud(const std::string &fname)
    : filename(fname), translationPre(0, 0, 0), euler(0, 0, 0),
      translationPost(0, 0, 0), matrix(1.0f), rawMatrix(false), hidden(false) {
  loadNumPoints();
  setRandomColor();
}

PointCloud::PointCloud(const std::filesystem::path &baseDirectory,
                       const json &j) {
  j.at("name").get_to(name);
  filename = (baseDirectory / (name + ".dat")).string();
  j.at("translationPre").get_to(translationPre);
  j.at("euler").get_to(euler);
  j.at("translationPost").get_to(translationPost);
  j.at("matrix").get_to(matrix);
  j.at("rawMatrix").get_to(rawMatrix);
  j.at("hidden").get_to(hidden);
  j.at("color").get_to(color);
  loadNumPoints();
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

void PointCloud::load(std::vector<char> &data) const {
  std::ifstream stream(filename.c_str());
  data.insert(data.end(), std::istreambuf_iterator<char>(stream),
              std::istreambuf_iterator<char>());
}

void PointCloud::loadNumPoints() {
  std::vector<char> data;
  load(data);
  numPoints = data.size() / (6 * 4);
}

void PointCloud::setRandomColor() {
  std::uniform_real_distribution<float> d(0.0, 1.0);
  color.x = d(rng);
  color.y = d(rng);
  color.z = d(rng);
}

json PointCloud::toJson() const {
  return {{"name", name},          {"translationPre", translationPre},
          {"euler", euler},        {"translationPost", translationPost},
          {"matrix", getMatrix()}, {"rawMatrix", rawMatrix},
          {"hidden", hidden},      {"color", color}};
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

  fs::path dataPath = getDataFile();
  if (fs::exists(dataPath)) {
    std::ifstream data(dataPath);
    json j;
    data >> j;
    for (const auto &p : j) {
      clouds.emplace_back(dataDirectory, p);
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
  std::vector<char> buffer;
  buffer.assign((char *)axes, ((char *)axes) + sizeof(axes));
  for (const auto &cloud : clouds) {
    cloud.load(buffer);
  }
  glBindVertexArray(mVAO);
  glBufferData(GL_ARRAY_BUFFER, buffer.size(), buffer.data(), GL_STATIC_DRAW);
  glBindVertexArray(0);
}

void Scene::render(const glm::mat4 &pv) const {
  mShader.use();
  glBindVertexArray(mVAO);

  glUniformMatrix4fv(mPvmLoc, 1, GL_FALSE, glm::value_ptr(pv));
  glUniform1i(mPaintUniformLoc, 0);
  glDrawArrays(GL_LINES, 0, 6);
  glUniform1i(mPaintUniformLoc, paintUniform);

  GLsizei offset = 6;
  for (const auto &pcd : clouds) {
    if (!pcd.hidden) {
      glm::mat4 pvm = pv * pcd.getMatrix();
      glUniformMatrix4fv(mPvmLoc, 1, GL_FALSE, glm::value_ptr(pvm));
      glUniform3fv(mUniformColorLoc, 1, glm::value_ptr(pcd.color));
      glDrawArrays(GL_POINTS, offset, pcd.numPoints);
    }
    offset += pcd.numPoints;
  }
  glBindVertexArray(0);
}
