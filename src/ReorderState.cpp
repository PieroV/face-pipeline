/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "ReorderState.h"

#include "imgui.h"

#include "glm/gtx/norm.hpp"

#include "strnatcmp.h"

#include "EditorState.h"

ReorderState::ReorderState(Application &app) : mApp(app) {}

void ReorderState::createGui() {
  if (ImGui::Begin("Reorder frames")) {
    auto &clouds = mApp.getScene().clouds;
    ImGui::TextUnformatted(
        "Drag and drop names or choose an option at the bottom.");

    int count = static_cast<int>(clouds.size());
    for (int i = 0; i < count; i++) {
      ImGui::Selectable(clouds[i].name.c_str());
      // Based on "Widgets/Drag and Drop/Drag to reorder items (simple)"
      if (ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
        int next = i + (ImGui::GetMouseDragDelta(0).y < 0 ? -1 : 1);
        if (next >= 0 && next < count) {
          std::swap(clouds[i], clouds[next]);
          ImGui::ResetMouseDragDelta();
        }
      }
    }

    if (ImGui::Button("Sort by name")) {
      std::stable_sort(clouds.begin(), clouds.end(),
                       [](const PointCloud &a, const PointCloud &b) {
                         return strnatcasecmp(a.name.c_str(), b.name.c_str()) <
                                0;
                       });
    }
    ImGui::SameLine();
    if (ImGui::Button("Minimize distances")) {
      minimizeDistances();
    }

    if (ImGui::Button("Close")) {
      mApp.setState(std::make_unique<EditorState>(mApp));
    }
  }
  ImGui::End();
}

void ReorderState::minimizeDistances() {
  auto &clouds = mApp.getScene().clouds;
  std::vector<float> distances(clouds.size());
  for (size_t i = 0; i < distances.size() - 1; i++) {
    glm::vec4 pos = clouds[i].matrix[3];
    for (size_t j = i + 1; j < distances.size(); j++) {
      distances[j] = glm::distance2(pos, clouds[j].matrix[3]);
    }
    ptrdiff_t closest =
        std::min_element(distances.begin() + i + 1, distances.end()) -
        distances.begin();
    assert(closest >= 0 && static_cast<size_t>(closest) < distances.size());
    if (static_cast<size_t>(closest) != (i + 1)) {
      std::swap(clouds[i + 1], clouds[closest]);
    }
  }
}
