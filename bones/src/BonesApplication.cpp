/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "BonesApplication.h"

#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/norm.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "ImGuizmo.h"

#include "open3d/io/TriangleMeshIO.h"

#include "shader.h"

Eigen::MatrixXd computeBoneWeights(const open3d::geometry::TriangleMesh &mesh,
                                   const std::vector<glm::dvec3> &bonePos,
                                   const std::vector<int> &bonePairs);

// TODO: Should this take density into account?
constexpr float handleRadius = 8;

constexpr int vertexFloats = 3 + 3 + 4 + 4;

BonesApplication::BonesApplication()
    : BaseApplication("Bones"), mRootBone(std::make_shared<Bone>()),
      mShader(createShader()) {
  assert(mRootBone);
  mRootBone->name = "root";

  static const char *uniformNames[U_Max] = {"pv", "bones", "skinned",
                                            "paintUniform", "uniformColor"};
  mShader.getUniformLocations(uniformNames, mUniforms, U_Max);

  const GLsizei stride = vertexFloats * static_cast<GLsizei>(sizeof(float));
  glBindVertexArray(mGlObjects.vao);
  glBindBuffer(GL_ARRAY_BUFFER, mGlObjects.vbo);
  // Position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
  glEnableVertexAttribArray(0);
  // Color
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
  // Bone indices
  glVertexAttribIPointer(2, 4, GL_INT, stride, (void *)(8 * sizeof(float)));
  glEnableVertexAttribArray(2);
  // Bone weights
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
                        (void *)(12 * sizeof(float)));
  glEnableVertexAttribArray(3);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mGlObjects.ebo);
  glBindVertexArray(0);

  ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
}

void BonesApplication::createGui() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(300, 400));
  if (ImGui::Begin("Bones")) {
    ImGui::InputText("Mesh", &mMeshPath);
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
      open3d::io::ReadTriangleMesh(mMeshPath, mMesh);
      mMesh.RemoveDegenerateTriangles();
      mMesh.RemoveNonManifoldEdges();
      mMesh.MergeCloseVertices(1e-4);
      uploadMesh();
    }
    ImGui::RadioButton("Edit skeleton", &mMode, M_Edit);
    ImGui::RadioButton("Pose skeleton", &mMode, M_Pose);
    if (mAddingChild && mMode != M_Edit) {
      mAddingChild = false;
    }
    if (mSelectedBone && mMode == M_Edit) {
      ImGui::InputText("Name", &mSelectedBone->name);
      ImGui::BeginDisabled(mAddingChild);
      if (ImGui::Button("Add child")) {
        mAddingChild = true;
      }
      ImGui::EndDisabled();
      if (mAddingChild) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          mAddingChild = false;
        }
      }
      ImGui::BeginDisabled(mSelectedBone == mRootBone);
      if (ImGui::Button("Delete")) {
        mSelectedBone->destroy();
        mSelectedBone = nullptr;
      }
      ImGui::EndDisabled();
    } else if (mSelectedBone) {
      ImGui::Text("Selected bone: %s", mSelectedBone->name.c_str());
    }
    if (mMode == M_Edit && ImGui::Button("Compute weights")) {
      computeWeights();
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();

  ImGuizmo::BeginFrame();
  ImGuiIO &io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

  updateHandles();

  if (mSelectedBone && mMode == M_Edit) {
    ImGuizmo::Manipulate(glm::value_ptr(mView), glm::value_ptr(mProjection),
                         ImGuizmo::TRANSLATE | ImGuizmo::ROTATE,
                         ImGuizmo::LOCAL, glm::value_ptr(mSelectedBone->world));
    mSelectedBone->localFromWorld(true);
  }
}

void BonesApplication::mouseClickCallback(int button, int action, int mods) {
  if (button != GLFW_MOUSE_BUTTON_1) {
    BaseApplication::mouseClickCallback(button, action, mods);
    return;
  }
  if (ImGui::GetIO().WantCaptureMouse || action == GLFW_RELEASE) {
    return;
  }

  if (mAddingChild) {
    mAddingChild = false;
    if (mHoveredBone) {
      return;
    }

    double mouseX, mouseY;
    glfwGetCursorPos(mWindow, &mouseX, &mouseY);
    int width, height;
    glfwGetFramebufferSize(mWindow, &width, &height);
    assert(width > 0 && height > 0);
    glm::vec4 ndcPos(2.0 * mouseX / width - 1, 1 - 2.0 * mouseY / height, -1.0,
                     1.0);
    glm::mat4 invPv = glm::inverse(mProjection * mView);
    glm::vec4 nearPos = invPv * ndcPos;
    if (nearPos.w < 1e-5) {
      return;
    }
    nearPos /= nearPos.w;
    ndcPos.z = 1.0f;
    glm::vec4 farPos = invPv * ndcPos;
    if (farPos.w < 1e-5) {
      return;
    }
    farPos /= farPos.w;

    glm::vec3 start(nearPos);
    glm::vec3 dir = glm::vec3(farPos) - start;
    float dist = glm::length(dir);
    if (dist < 1e-5f) {
      return;
    }
    dir /= dist;
    glm::vec3 p = glm::vec3(mSelectedBone->world[3]) - start;
    float dot = glm::dot(p, dir);
    p = dot * dir + start;

    auto child = mSelectedBone->addChild();
    child->name = "Child " + std::to_string(mSelectedBone->children.size());
    child->world[3] = glm::vec4(p, 1.0f);
    child->localFromWorld();
    mSelectedBone = child;
  } else {
    // The hovered bone might be nullptr, and it's what we want: in case we will
    // cancel the current selection.
    mSelectedBone = mHoveredBone;
  }
}

struct BoneHandle {
  BoneHandle(const std::shared_ptr<Bone> &owner) {
    assert(owner);
    bone = owner;
    pos.x = bone->screen.x;
    pos.y = bone->screen.y;
    z = bone->screen.z;
  }

  BoneHandle(Bone &b) : BoneHandle(b.shared_from_this()) {}

  bool hovering(const glm::vec2 &mousePos) const {
    return glm::distance2({pos.x, pos.y}, mousePos) <=
           (handleRadius * handleRadius);
  }

  bool operator<(const BoneHandle &other) const {
    // We want to order from further from the screen to closest, and Z is
    // negative towards the screen in NDC.
    return z > other.z;
  }

  std::shared_ptr<Bone> bone;
  ImVec2 pos;
  float z;
};

void BonesApplication::updateHandles() {
  mRootBone->propagateMatrix();

  glm::mat4 pv = mProjection * mView;
  glm::ivec2 windowSize;
  glfwGetFramebufferSize(mWindow, &windowSize.x, &windowSize.y);
  std::vector<std::pair<ImVec2, ImVec2>> lines;
  std::vector<BoneHandle> handles;
  mRootBone->traverse([&](Bone &b) {
    std::shared_ptr<Bone> parent = b.parent.lock();
    b.screen = (pv * b.world)[3];
    if (fabs(b.screen.w) > 1e-5) {
      b.screen /= b.screen.w;
      b.screen.x = (b.screen.x + 1) * 0.5f * windowSize.x;
      b.screen.y = (1 - b.screen.y) * 0.5f * windowSize.y;
      handles.emplace_back(b);
      if (parent) {
        lines.push_back(
            {{parent->screen.x, parent->screen.y}, {b.screen.x, b.screen.y}});
      }
    }
  });

  ImDrawList *dl = ImGui::GetBackgroundDrawList();
  ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_Border);
  for (const auto &pair : lines) {
    dl->AddLine(pair.first, pair.second, lineColor, 2);
  }

  // Draw handles from further to closest.
  std::sort(handles.begin(), handles.end());
  glm::dvec2 mousePos;
  glfwGetCursorPos(mWindow, &mousePos.x, &mousePos.y);
  // Only one can be hovered, so do it in two loops.
  mHoveredBone = nullptr;
  for (const BoneHandle &bh : handles) {
    bool hovered =
        mMouseCaptured == MouseMovement::None && bh.hovering(mousePos);
    if (hovered) {
      mHoveredBone = bh.bone;
    }
  }
  for (const BoneHandle &bh : handles) {
    ImU32 color;
    if (bh.bone == mSelectedBone) {
      color = ImGui::GetColorU32(ImGuiCol_ButtonActive);
    } else if (mHoveredBone == bh.bone) {
      color = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    } else {
      color = ImGui::GetColorU32(ImGuiCol_Button);
    }
    dl->AddCircleFilled(bh.pos, handleRadius, color);
  }
}

void BonesApplication::computeWeights() {
  std::unordered_map<std::shared_ptr<const Bone>, int> boneIndices;
  std::vector<glm::dvec3> bonePos;
  std::vector<int> bonePairs;
  mRootBone->traverse([&](const Bone &b) {
    auto pair = boneIndices.insert(
        {b.shared_from_this(), static_cast<int>(boneIndices.size())});
    std::shared_ptr<Bone> parent = b.parent.lock();
    bonePos.emplace_back(b.world[3]);
    if (parent) {
      bonePairs.push_back(boneIndices.at(parent));
      bonePairs.push_back(pair.first->second);
    }
  });

  computeBoneWeights(mMesh, bonePos, bonePairs);
}

void BonesApplication::render() {
  glm::mat4 pv = mProjection * mView;
  if (mNumIndices > 0) {
    mShader.use();
    glUniformMatrix4fv(mUniforms[U_PV], 1, GL_FALSE, glm::value_ptr(pv));
    glUniform1i(mUniforms[U_Skinned], 0);
    glUniform1i(mUniforms[U_PaintUniform], 0);
    glBindVertexArray(mGlObjects.vao);
    glDrawElements(GL_TRIANGLES, mNumIndices, GL_UNSIGNED_INT, nullptr);
  }
}

void BonesApplication::uploadMesh() {
  mNumIndices = 0;
  if (mMesh.IsEmpty()) {
    return;
  }

  using namespace Eigen;
  MatrixXf vertices = MatrixXf::Zero(vertexFloats, mMesh.vertices_.size());
  Map<const MatrixXd> points(mMesh.vertices_.front().data(), 3,
                             mMesh.vertices_.size());
  vertices.block(0, 0, 3, vertices.cols()) = points.cast<float>();
  if (mMesh.vertex_colors_.size() == mMesh.vertices_.size()) {
    Map<const MatrixXd> colors(mMesh.vertex_colors_.front().data(), 3,
                               mMesh.vertex_colors_.size());
    vertices.block(3, 0, 3, vertices.cols()) = colors.cast<float>();
  }

  glBindVertexArray(mGlObjects.vao);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float),
               vertices.data(), GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               mMesh.triangles_.size() * 3 * sizeof(int),
               mMesh.triangles_.data(), GL_STATIC_DRAW);
  glBindVertexArray(0);
  mNumIndices = static_cast<GLsizei>(mMesh.triangles_.size() * 3);
}
