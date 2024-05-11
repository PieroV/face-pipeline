/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "glm/glm.hpp"

struct Bone : public std::enable_shared_from_this<Bone> {
  Bone() = default;
  Bone(const Bone &other) = delete;
  Bone(Bone &&other) = delete;
  Bone &operator=(const Bone &other) = delete;
  Bone &operator=(Bone &&other) = delete;

  std::shared_ptr<Bone> addChild();
  void traverse(std::function<void(Bone &)> callback);
  void propagateMatrix();
  void localFromWorld(bool propagate = true);
  void destroy();

  std::string name;
  glm::mat4 local{1.0f};
  glm::mat4 world{1.0f};
  glm::vec4 screen{0.0f};
  std::weak_ptr<Bone> parent;
  std::vector<std::shared_ptr<Bone>> children;
};
