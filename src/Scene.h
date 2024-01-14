#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "glad/glad.h"

#include "glm/glm.hpp"

#include "nlohmann/json.hpp"

#include "shaders.h"

class PointCloud {
public:
  PointCloud(const std::string &fname);
  PointCloud(const std::filesystem::path &baseDirectory,
             const nlohmann::json &j);

  void load(std::vector<char> &data) const;
  glm::mat4 getMatrix() const;
  nlohmann::json toJson() const;

private:
  void loadNumPoints();
  void setRandomColor();

public:
  std::string name;
  std::string filename;
  glm::vec3 translationPre;
  glm::vec3 euler;
  glm::vec3 translationPost;
  glm::vec3 color;
  glm::mat4 matrix;
  bool rawMatrix;
  bool hidden;
  GLsizei numPoints = 0;
};

class Scene {
public:
  Scene(const Scene &other) = delete;
  Scene(Scene &&other) noexcept;
  Scene &operator=(const Scene &other) = delete;
  Scene &operator=(Scene &&other) noexcept;
  ~Scene();
  static std::pair<std::unique_ptr<Scene>, std::vector<std::string>>
  load(const std::filesystem::path &dataDirectory);
  void save() const;

  void refreshBuffer();
  void render(const glm::mat4 &pv) const;

  std::vector<PointCloud> clouds;
  bool paintUniform = false;

private:
  Scene() = default;
  Scene(const std::filesystem::path &dataDirectory);
  std::vector<std::string> loadFrames();
  std::filesystem::path getDataFile() const;

  std::filesystem::path mDataDirectory;

  // We will create the shader each time we create a Scene instance.
  // This is not optimal, but it is okay to do it for now, maybe we can do
  // something better in the future.
  ShaderProgram mShader;

  GLuint mVAO = 0;
  GLuint mVBO = 0;
  GLint mPvmLoc = -1;
  GLint mPaintUniformLoc = -1;
  GLint mUniformColorLoc = -1;
};
