/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "EditorState.h"

#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "AddFrameState.h"
#include "AlignState.h"
#include "GlobalAlignState.h"
#include "MergeState.h"
#include "NoiseRemovalState.h"
#include "ReorderState.h"
#include "TextureLabState.h"
#include "utilities.h"

EditorState::EditorState(Application &app)
    : mApp(app), mScene(app.getScene()) {}

void EditorState::start() { refreshBuffer(); }

void EditorState::createGui() {
  createMain();
  if (mEditing) {
    createEdit();
  }
  if (mMultiEditing) {
    createMultiEdit();
  }
}

void EditorState::createMain() {
  Scene &scene = mApp.getScene();
  auto &clouds = scene.clouds;

  ImGui::Begin("Main");

  if (ImGui::BeginTable("clouds-table", 6)) {
    ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Hide", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Remove noise", ImGuiTableColumnFlags_WidthFixed);
    for (size_t i = 0; i < clouds.size(); i++) {
      ImGui::TableNextColumn();
      // Since we have an ordered set we could do something smart with an
      // iterator, but we do not expect to have many elements in it, so this is
      // probably fine.
      auto it = mSelected.find(i);
      bool selected = it != mSelected.end();
      ImGui::BeginDisabled(mMultiEditing);
      ImGui::Checkbox(std::to_string(i).c_str(), &selected);
      ImGui::EndDisabled();
      if (selected && it == mSelected.end()) {
        mSelected.insert(i);
      } else if (!selected && it != mSelected.end()) {
        mSelected.erase(it);
      }
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(clouds[i].name.c_str());
      ImGui::TableNextColumn();
      char id[50];
      const auto &c = clouds[i].color;
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.x, c.y, c.z, 1.0f));
      snprintf(id, sizeof(id), "Edit##%zu", i);
      if (ImGui::Button(id)) {
        beginEdit(i);
      }
      ImGui::PopStyleColor(1);
      ImGui::TableNextColumn();
      snprintf(id, sizeof(id), "Delete##%zu", i);
      if (ImGui::Button(id)) {
        if (mEditing && mEditIndex == i) {
          mEditing = false;
        }
        clouds.erase(clouds.begin() + i);
        mSelected.erase(i);
        mMultiEditMatrices.erase(i);
        i--;
        refreshBuffer();
      }
      ImGui::TableNextColumn();
      snprintf(id, sizeof(id), "Hidden##%zu", i);
      ImGui::Checkbox(id, &clouds[i].hidden);
      ImGui::TableNextColumn();
      snprintf(id, sizeof(id), "Remove noise##%zu", i);
      if (ImGui::Button(id)) {
        mApp.setState(std::make_unique<NoiseRemovalState>(mApp, clouds[i]));
      }
    }
    ImGui::EndTable();
  }

  ImGui::BeginDisabled(mMultiEditing);
  if (ImGui::Button("Select all")) {
    for (size_t i = 0; i < clouds.size(); i++) {
      mSelected.insert(i);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Select none")) {
    mSelected.clear();
  }
  ImGui::SameLine();
  if (ImGui::Button("Select visible")) {
    mSelected.clear();
    for (size_t i = 0; i < clouds.size(); i++) {
      if (!clouds[i].hidden) {
        mSelected.insert(i);
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Invert selection")) {
    std::set<size_t> inverted;
    for (size_t i = 0; i < clouds.size(); i++) {
      if (!mSelected.count(i)) {
        inverted.insert(i);
      }
    }
    mSelected.swap(inverted);
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(mSelected.empty());
  if (ImGui::Button("Edit multiple")) {
    beginMultiEdit();
  }
  if (ImGui::Button("Show selected")) {
    for (size_t idx : mSelected) {
      assert(idx < clouds.size());
      clouds[idx].hidden = false;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Hide selected")) {
    for (size_t idx : mSelected) {
      assert(idx < clouds.size());
      clouds[idx].hidden = true;
    }
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(mSelected.size() != 2);
  if (ImGui::Button("Align")) {
    assert(mSelected.size() == 2);
    auto it = mSelected.begin();
    size_t a = *it++;
    size_t b = *it;
    mApp.setState(std::make_unique<AlignState>(mApp, a, b));
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(mSelected.size() < 2);
  if (ImGui::Button("Global align")) {
    mApp.setState(std::make_unique<GlobalAlignState>(mApp, mSelected));
  }
  ImGui::EndDisabled();

  ImGui::BeginDisabled(mSelected.empty());
  if (ImGui::Button("Merge")) {
    mApp.setState(std::make_unique<MergeState>(mApp, mSelected));
  }
  if (ImGui::Button("Texture lab")) {
    mApp.setState(std::make_unique<TextureLabState>(mApp, mSelected));
  }
  ImGui::EndDisabled();

  if (ImGui::Button("Add")) {
    mApp.setState(std::make_unique<AddFrameState>(mApp));
  }
  ImGui::SameLine();
  if (ImGui::Button("Reorder")) {
    mApp.setState(std::make_unique<ReorderState>(mApp));
  }

  if (ImGui::Button("Save")) {
    scene.save();
  }

  ImGui::Checkbox("Paint uniform", &mPaintUniform);

  Renderer::Symmetry &mirror = mApp.getRenderer().mirror;
  assert(mirror >= 0 && mirror < Renderer::MirrorMax);
  static const char *symLabels[Renderer::MirrorMax] = {
      "No symmetry", "Mirror on negative X", "Mirror on positive X"};
  if (ImGui::BeginCombo("Symmetry", symLabels[mirror])) {
    for (int i = 0; i < Renderer::MirrorMax; i++) {
      bool isSelected = (mirror == i);
      if (ImGui::Selectable(symLabels[i], isSelected)) {
        mirror = static_cast<Renderer::Symmetry>(i);
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  bool voxelChanged =
      ImGui::Checkbox("Voxel down for visualization", &mVoxelDown);
  voxelChanged = ImGui::InputDouble("Voxel size", &mVoxelSize) || voxelChanged;
  if (voxelChanged && mVoxelDown && mVoxelSize <= 0) {
    mVoxelDown = false;
  }
  if (voxelChanged) {
    refreshBuffer();
  }

  ImGui::End();
}

void EditorState::createEdit() {
  Scene &scene = mApp.getScene();
  if (mEditIndex >= scene.clouds.size()) {
    mEditing = false;
    return;
  }
  PointCloud &cloud = scene.clouds[mEditIndex];

  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(400, 120));
  if (ImGui::Begin("Edit", &mEditing)) {
    if (ImGui::InputDouble("Depth max value", &cloud.trunc)) {
      cloud.loadData(scene);
      refreshBuffer();
    }
    ImGui::ColorEdit3("Color", glm::value_ptr(cloud.color));
    if (ImGui::Button("New random color")) {
      cloud.color = randomColor();
    }
    glm::mat4 rowMajor = glm::transpose(mEditMatrix);
    ImGui::InputFloat4("Row 0", glm::value_ptr(rowMajor[0]));
    ImGui::InputFloat4("Row 1", glm::value_ptr(rowMajor[1]));
    ImGui::InputFloat4("Row 2", glm::value_ptr(rowMajor[2]));
    ImGui::InputFloat4("Row 3", glm::value_ptr(rowMajor[3]));
    mEditMatrix = glm::transpose(rowMajor);
    cloud.matrix = multiTransformUi() * mEditMatrix;
    if (ImGui::Button("Update matrix")) {
      mEditMatrix = cloud.matrix;
      mTransformations.clear();
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

void EditorState::createMultiEdit() {
  auto &clouds = mApp.getScene().clouds;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(400, 120));
  if (ImGui::Begin("Edit multiple", &mMultiEditing)) {
    glm::mat4 matrix = multiTransformUi();
    for (auto &pair : mMultiEditMatrices) {
      assert(pair.first < clouds.size());
      clouds[pair.first].matrix = matrix * pair.second;
    }
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

void EditorState::render(const glm::mat4 &pv) {
  mApp.renderScene(pv, mPaintUniform);
}

bool EditorState::keyCallback(int key, int scancode, int action, int mods) {
  (void)scancode;
  Renderer::Symmetry &mirror = mApp.getRenderer().mirror;
  if (key == GLFW_KEY_M && action == GLFW_PRESS) {
    bool back = mods & GLFW_MOD_SHIFT;
    switch (mirror) {
    case Renderer::MirrorNone:
      mirror = back ? Renderer::MirrorOnPosX : Renderer::MirrorOnNegX;
      break;
    case Renderer::MirrorOnNegX:
      mirror = back ? Renderer::MirrorNone : Renderer::MirrorOnPosX;
      break;
    case Renderer::MirrorOnPosX:
      mirror = back ? Renderer::MirrorOnNegX : Renderer::MirrorNone;
      break;
    default:
      assert("Invalid mirror value" && false);
    }
    return true;
  }
  if (key == GLFW_KEY_S && (mods & GLFW_MOD_CONTROL) && action == GLFW_PRESS) {
    mScene.save();
    return true;
  }
  return false;
}

glm::mat4 EditorState::multiTransformUi() {
  glm::mat4 matrix(1.0f);
  for (size_t i = 0; i < mTransformations.size(); i++) {
    auto &tr = mTransformations[i];
    char deleteLabel[15];
    snprintf(deleteLabel, sizeof(deleteLabel), "X##%zu", i);
    char label[30];
    switch (tr.first) {
    case Transformation::Translation:
      snprintf(label, sizeof(label), "Translation##%zu", i);
      ImGui::InputFloat3(label, glm::value_ptr(tr.second));
      ImGui::SameLine();
      if (ImGui::Button(deleteLabel)) {
        mTransformations.erase(mTransformations.begin() + i);
        i--;
        continue;
      }
      matrix = glm::translate(glm::mat4(1.0f), tr.second) * matrix;
      break;
    case Transformation::Rotation:
      snprintf(label, sizeof(label), "Rotation##%zu", i);
      ImGui::DragFloat3(label, glm::value_ptr(tr.second), 0.5f, -360.0f,
                        360.0f);
      ImGui::SameLine();
      if (ImGui::Button(deleteLabel)) {
        mTransformations.erase(mTransformations.begin() + i);
        i--;
        continue;
      }
      matrix = glm::eulerAngleYXZ(glm::radians(tr.second.y),
                                  glm::radians(tr.second.x),
                                  glm::radians(tr.second.z)) *
               matrix;
      break;
    }
  }

  if (ImGui::Button("Add rotation")) {
    mTransformations.push_back({Transformation::Rotation, {0.0f, 0.0f, 0.0f}});
  }
  ImGui::SameLine();
  if (ImGui::Button("Add translation")) {
    mTransformations.push_back(
        {Transformation::Translation, {0.0f, 0.0f, 0.0f}});
  }
  if (ImGui::Button("Remove all")) {
    mTransformations.clear();
    return glm::mat4(1.0f);
  }

  return matrix;
}

void EditorState::beginEdit(size_t idx) {
  mEditing = true;
  const Scene &scene = mApp.getScene();
  assert(idx < scene.clouds.size());
  mEditIndex = idx;
  mEditMatrix = scene.clouds[idx].matrix;
  mTransformations.clear();
  mMultiEditing = false;
}

void EditorState::beginMultiEdit() {
  auto &clouds = mApp.getScene().clouds;
  mEditing = false;
  mMultiEditing = true;
  mMultiEditMatrices.clear();
  for (size_t i : mSelected) {
    assert(i < clouds.size());
    // We need to force the point clouds to switch to the raw matrix.
    mMultiEditMatrices.insert({i, clouds[i].matrix});
  }
  mTransformations.clear();
}

void EditorState::refreshBuffer() {
  std::optional<double> voxelSize;
  if (mVoxelDown && mVoxelSize > 0) {
    voxelSize.emplace(mVoxelSize);
  }
  mApp.refreshBuffer(voxelSize);
}
