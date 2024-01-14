/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "LoadState.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "EditorState.h"

LoadState::LoadState(Application &app, std::unique_ptr<Scene> &scene,
                     const char *dataDirectory)
    : mApp(app), mScene(scene), mDirectory(dataDirectory ? dataDirectory : "") {
  if (dataDirectory) {
    load();
  }
}

void LoadState::createGui() {
  if (!mError.empty()) {
    const char title[] = "Load Error";
    ImGui::OpenPopup(title);
    bool open = true;
    if (ImGui::BeginPopupModal(title, &open)) {
      ImGui::TextUnformatted("Failed to open the dataset.");
      ImGui::TextUnformatted(mError.c_str());
      if (ImGui::Button("OK##Load Error")) {
        open = false;
      }
      ImGui::EndPopup();
    }
    if (!open) {
      mError.clear();
    }
  } else if (!mWarnings.empty()) {
    const char title[] = "Load Warnings";
    ImGui::OpenPopup(title);
    bool open = true;
    if (ImGui::BeginPopupModal(title, &open)) {
      ImGui::TextUnformatted("Some waring occurred while opening the dataset.");
      for (const std::string &warning : mWarnings) {
        ImGui::TextUnformatted(warning.c_str());
      }
      if (ImGui::Button("OK##Load Warnings")) {
        open = false;
      }
      ImGui::EndPopup();
    }
    if (!open) {
      mWarnings.clear();
      switchToEditor();
    }
  } else {
    ImGui::Begin("Load dataset", nullptr, ImGuiWindowFlags_NoTitleBar);
    ImGui::InputText("Data directory", &mDirectory);
    ImGui::BeginDisabled(mDirectory.empty());
    if (ImGui::Button("Load")) {
      load();
    }
    ImGui::EndDisabled();
    ImGui::End();
  }
}

void LoadState::load() {
  if (mDirectory.empty()) {
    mError = "The data directory is empty.";
    return;
  }
  try {
    std::tie(mScene, mWarnings) = Scene::load(mDirectory);
    if (mWarnings.empty()) {
      switchToEditor();
    }
  } catch (std::exception &e) {
    mError = e.what();
  }
}

void LoadState::switchToEditor() {
  mApp.setState(std::make_unique<EditorState>(mApp));
}
