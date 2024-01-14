#include "EditorState.h"

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

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
    ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("Hide", ImGuiTableColumnFlags_WidthFixed);
    for (size_t i = 0; i < scene.clouds.size(); i++) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%zu %s", i, scene.clouds[i].filename.c_str());
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
    mPcdFilename.clear();
    ImGui::OpenPopup("Add point cloud");
  }

  // Always center this window when appearing
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Add point cloud", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename", &mPcdFilename);
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      scene.clouds.emplace_back(mPcdFilename);
      scene.refreshBuffer();
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ImGui::Button("Save")) {
    scene.save();
  }

  ImGui::Checkbox("Paint uniform", &scene.paintUniform);
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
    ImGui::End();
  }
  ImGui::PopStyleVar();
}

void EditorState::render(const glm::mat4 &pv) { mApp.getScene().render(pv); }
