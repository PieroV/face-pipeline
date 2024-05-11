/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "Bone.h"

#include <deque>
#include <stdexcept>

#include <cassert>

std::shared_ptr<Bone> Bone::addChild() {
  std::shared_ptr<Bone> child = std::make_shared<Bone>();
  assert(child);
  child->parent = shared_from_this();
  children.push_back(child);
  return child;
}

void Bone::traverse(std::function<void(Bone &)> callback) {
  std::deque<std::shared_ptr<Bone>> q({{shared_from_this()}});
  while (!q.empty()) {
    std::shared_ptr<Bone> b;
    b.swap(q.front());
    assert(b);
    q.pop_front();
    q.insert(q.end(), b->children.begin(), b->children.end());
    callback(*b);
  }
}

void Bone::propagateMatrix() {
  traverse([&](Bone &b) {
    std::shared_ptr<Bone> parent = b.parent.lock();
    if (parent) {
      b.world = parent->world * b.local;
    }
  });
}

void Bone::localFromWorld(bool propagate) {
  std::shared_ptr<Bone> p = parent.lock();
  if (p) {
    local = glm::inverse(p->world) * world;
  }
  if (propagate) {
    propagateMatrix();
  }
}

void Bone::destroy() {
  std::shared_ptr<Bone> p = parent.lock();
  if (!p) {
    throw std::invalid_argument("Refusing to delete the root bone.");
  }
  for (auto &child : children) {
    child->parent = parent;
    child->local = local * child->local;
    p->children.push_back(child);
  }
  children.clear();
  // Assumption: each child is contained exactly one time.
  auto it =
      std::find(p->children.begin(), p->children.end(), shared_from_this());
  assert(it != p->children.end());
  p->children.erase(it);
}
