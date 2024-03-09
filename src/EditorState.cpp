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

EditorState::EditorState(Application &app)
    : mApp(app), mScene(app.getScene()) {}

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

  ImGui::Begin("Main");

  if (ImGui::BeginTable("clouds-table", 5)) {
    ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Hide", ImGuiTableColumnFlags_WidthFixed);
    for (size_t i = 0; i < scene.clouds.size(); i++) {
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
      ImGui::TextUnformatted(scene.clouds[i].name.c_str());
      ImGui::TableNextColumn();
      char id[50];
      const auto &c = scene.clouds[i].color;
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
        scene.clouds.erase(scene.clouds.begin() + i);
        mSelected.erase(i);
        mMultiEditMatrices.erase(i);
        i--;
        scene.refreshBuffer();
      }
      ImGui::TableNextColumn();
      snprintf(id, sizeof(id), "Hidden##%zu", i);
      ImGui::Checkbox(id, &scene.clouds[i].hidden);
    }
    ImGui::EndTable();
  }

  ImGui::BeginDisabled(mMultiEditing);
  if (ImGui::Button("Select all")) {
    for (size_t i = 0; i < scene.clouds.size(); i++) {
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
    for (size_t i = 0; i < scene.clouds.size(); i++) {
      if (!scene.clouds[i].hidden) {
        mSelected.insert(i);
      }
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

  ImGui::BeginDisabled(mSelected.empty());
  if (ImGui::Button("Edit multiple")) {
    beginMultiEdit();
  }
  ImGui::EndDisabled();

  if (ImGui::Button("Add")) {
    mApp.setState(std::make_unique<AddFrameState>(mApp));
  }

  if (ImGui::Button("Save")) {
    scene.save();
  }

  ImGui::Checkbox("Paint uniform", &scene.paintUniform);

  assert(mScene.mirror >= 0 && mScene.mirror < Scene::MirrorMax);
  static const char *symLabels[Scene::MirrorMax] = {
      "No symmetry", "Mirror on negative X", "Mirror on positive X"};
  if (ImGui::BeginCombo("Symmetry", symLabels[mScene.mirror])) {
    for (int i = 0; i < Scene::MirrorMax; i++) {
      bool isSelected = (mScene.mirror == i);
      if (ImGui::Selectable(symLabels[i], isSelected)) {
        mScene.mirror = static_cast<Scene::Symmetry>(i);
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  bool voxelChanged =
      ImGui::Checkbox("Voxel down for visualization", &scene.voxelDown);
  voxelChanged =
      ImGui::InputDouble("Voxel size", &scene.voxelSize) || voxelChanged;
  if (voxelChanged && scene.voxelDown && scene.voxelSize <= 0) {
    scene.voxelDown = false;
  }
  if (voxelChanged) {
    mScene.refreshBuffer();
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
      scene.refreshBuffer();
    }
    ImGui::ColorEdit3("Color", glm::value_ptr(cloud.color));
    ImGui::Checkbox("Raw matrix", &cloud.rawMatrix);
    if (!cloud.rawMatrix) {
      ImGui::InputFloat3("Translation pre",
                         glm::value_ptr(cloud.translationPre));
      ImGui::DragFloat3("Rotation", glm::value_ptr(cloud.euler), 0.5f, -360.0f,
                        360.0f);
      ImGui::InputFloat3("Translation post",
                         glm::value_ptr(cloud.translationPost));
    } else {
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

void EditorState::render(const glm::mat4 &pv) { mApp.getScene().render(pv); }

bool EditorState::keyCallback(int key, int scancode, int action, int mods) {
  (void)scancode;
  if (key == GLFW_KEY_M && action == GLFW_PRESS) {
    bool back = mods & GLFW_MOD_SHIFT;
    switch (mScene.mirror) {
    case Scene::MirrorNone:
      mScene.mirror = back ? Scene::MirrorOnPosX : Scene::MirrorOnNegX;
      break;
    case Scene::MirrorOnNegX:
      mScene.mirror = back ? Scene::MirrorNone : Scene::MirrorOnPosX;
      break;
    case Scene::MirrorOnPosX:
      mScene.mirror = back ? Scene::MirrorOnNegX : Scene::MirrorNone;
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

void EditorState::beginEdit(size_t idx) {
  mEditing = true;
  const Scene &scene = mApp.getScene();
  assert(idx < scene.clouds.size());
  mEditIndex = idx;
  mEditMatrix = scene.clouds[idx].getMatrix();
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
    glm::mat4 matrix = clouds[i].getMatrix();
    mMultiEditMatrices.insert({i, matrix});
    clouds[i].rawMatrix = true;
  }
  mTransformations.clear();
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
