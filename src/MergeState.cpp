/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "MergeState.h"

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "open3d/io/PointCloudIO.h"
#include "open3d/io/TriangleMeshIO.h"
#include "open3d/pipelines/integration/UniformTSDFVolume.h"

#include "EditorState.h"

MergeState::MergeState(Application &app, const std::set<size_t> &indices)
    : mApp(app), mIndices(indices), mOrigin(0.0, 0.0, 0.0) {}

void MergeState::start() {
  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
}

void MergeState::createGui() {
  if (ImGui::Begin("Merge")) {
    ImGui::InputDouble("Length", &mLength);
    ImGui::InputInt("Resolution", &mResolution);
    double memory = pow(mResolution, 3) * sizeof(open3d::geometry::TSDFVoxel);
    char unit;
    if (memory > 1e9) {
      memory *= 1e-9;
      unit = 'G';
    } else if (memory > 1e6) {
      memory *= 1e-6;
      unit = 'M';
    } else {
      memory *= 1e-3;
      unit = 'k';
    }
    ImGui::Text("Requested memory: %.3f%cB", memory, unit);
    ImGui::Text("Voxel size: %f", mLength / mResolution);
    ImGui::InputDouble("SDF truncation value", &mSdfTrunc);
    ImGui::InputFloat3("Origin", mOrigin.data());

    if (ImGui::Button("Merge")) {
      const Scene &scene = mApp.getScene();
      const auto &intrinsic = scene.getCameraIntrinsic();
      const auto &clouds = scene.clouds;

      using namespace open3d::pipelines::integration;
      UniformTSDFVolume volume(mLength, mResolution, mSdfTrunc,
                               TSDFVolumeColorType::RGB8,
                               mOrigin.cast<double>());
      for (size_t idx : mIndices) {
        using namespace Eigen;
        const PointCloud &pcd = clouds.at(idx);
        Matrix4d matrix =
            Map<const Matrix4f>(glm::value_ptr(pcd.getMatrix())).cast<double>();
        matrix = matrix.inverse().eval();
        auto maybeMasked = pcd.getMaskedRgbd();
        volume.Integrate(maybeMasked ? *maybeMasked : pcd.getRgbdImage(),
                         intrinsic, matrix);
      }
      mPointCloud = volume.ExtractPointCloud();
      mMesh = volume.ExtractTriangleMesh();
      if (mPointCloud && mMesh) {
        // TODO: Signal the error otherwise.
        Renderer &r = mApp.getRenderer();
        r.clearBuffer();
        r.addPointCloud(*mPointCloud);
        r.addTriangleMesh(*mMesh);
        r.uploadBuffer();
      }
    }
  }

  ImGui::BeginDisabled(!mPointCloud);
  if (ImGui::Button("Export pointcloud...")) {
    ImGui::OpenPopup("Export pointcloud");
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!mMesh);
  if (ImGui::Button("Export mesh...")) {
    ImGui::OpenPopup("Export mesh");
  }
  ImGui::EndDisabled();

  if (ImGui::BeginPopupModal("Export pointcloud", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename", &mPcdFilename);
    if (ImGui::Button("Export")) {
      assert(mPointCloud);
      open3d::io::WritePointCloud(mPcdFilename, *mPointCloud);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (ImGui::BeginPopupModal("Export mesh", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Filename", &mMeshFilename);
    if (ImGui::Button("Export")) {
      assert(mMesh);
      open3d::io::WriteTriangleMesh(mMeshFilename, *mMesh);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  static const char *symLabels[] = {"Point cloud", "Mesh", "Mesh wireframe"};
  if (ImGui::BeginCombo("Render mode", symLabels[mRenderMode])) {
    for (int i = 0; i < 3; i++) {
      bool isSelected = (mRenderMode == i);
      if (ImGui::Selectable(symLabels[i], isSelected)) {
        mRenderMode = i;
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  if (ImGui::Button("Close")) {
    mApp.setState(std::make_unique<EditorState>(mApp));
  }
  ImGui::End();
}

void MergeState::render(const glm::mat4 &pv) {
  if (!mPointCloud || !mMesh) {
    return;
  }

  Renderer &r = mApp.getRenderer();
  r.beginRendering(pv);
  if (mRenderMode == 1 || mRenderMode == 2) {
    GLint polygonMode;
    glGetIntegerv(GL_POLYGON_MODE, &polygonMode);
    if (mRenderMode == 2) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    r.renderIndexedMesh(1, glm::mat4(1.0f));
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode);
  } else {
    r.renderPointCloud(0, glm::mat4(1.0f));
  }
  r.endRendering();
}
