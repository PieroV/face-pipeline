/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "EditorState.h"

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "AddFrameState.h"

EditorState::EditorState(Application &app)
    : mApp(app), mScene(app.getScene()) {}

void EditorState::createGui() {
  createMain();
  createEdit();
}

void EditorState::createMain() {
  Scene &scene = mApp.getScene();

  ImGui::Begin("Main");

  if (ImGui::BeginTable("clouds-table", 4)) {
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Hide", ImGuiTableColumnFlags_WidthFixed);
    for (size_t i = 0; i < scene.clouds.size(); i++) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%zu %s", i, scene.clouds[i].name.c_str());
      ImGui::TableSetColumnIndex(1);
      char id[50];
      const auto &c = scene.clouds[i].color;
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.x, c.y, c.z, 1.0f));
      snprintf(id, sizeof(id), "Edit##%zu", i);
      if (ImGui::Button(id)) {
        mEditing = true;
        mEditIndex = i;
      }
      ImGui::PopStyleColor(1);
      ImGui::TableSetColumnIndex(2);
      snprintf(id, sizeof(id), "Delete##%zu", i);
      if (ImGui::Button(id)) {
        if (mEditing && mEditIndex == i) {
          mEditing = false;
        }
        scene.clouds.erase(scene.clouds.begin() + i);
        i--;
        scene.refreshBuffer();
      }
      ImGui::TableSetColumnIndex(3);
      snprintf(id, sizeof(id), "Hidden##%zu", i);
      ImGui::Checkbox(id, &scene.clouds[i].hidden);
    }
    ImGui::EndTable();
  }

  if (ImGui::Button("Add")) {
    mApp.setState(std::make_unique<AddFrameState>(mApp));
  }

  // Always center this window when appearing
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::Button("Save")) {
    scene.save();
  }

  ImGui::Checkbox("Paint uniform", &scene.paintUniform);

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
  }
  if (!mEditing) {
    return;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(400, 120));
  if (ImGui::Begin("Edit", &mEditing)) {
    if (!scene.clouds[mEditIndex].rawMatrix) {
      ImGui::InputFloat3(
          "Translation pre",
          glm::value_ptr(scene.clouds[mEditIndex].translationPre));
      ImGui::DragFloat3("Rotation",
                        glm::value_ptr(scene.clouds[mEditIndex].euler), 0.5f,
                        -360.0f, 360.0f);
      ImGui::InputFloat3(
          "Translation post",
          glm::value_ptr(scene.clouds[mEditIndex].translationPost));
    }
    ImGui::ColorEdit3("Color", glm::value_ptr(scene.clouds[mEditIndex].color));
    ImGui::Checkbox("Raw matrix", &scene.clouds[mEditIndex].rawMatrix);
    if (ImGui::InputDouble("Depth max value",
                           &scene.clouds[mEditIndex].trunc)) {
      scene.clouds[mEditIndex].loadData(scene);
      scene.refreshBuffer();
    }
    ImGui::End();
  }
  ImGui::PopStyleVar();
}

void EditorState::render(const glm::mat4 &pv) { mApp.getScene().render(pv); }
