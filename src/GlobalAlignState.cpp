/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "GlobalAlignState.h"

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"

#include "open3d/pipelines/registration/FastGlobalRegistration.h"

#include "EditorState.h"

using namespace open3d::pipelines::registration;

GlobalAlignState::GlobalAlignState(Application &app,
                                   const std::set<size_t> &indices)
    : mApp(app), mIndices(indices.begin(), indices.end()),
      mNormalsParams(0.1, 30), mSearchParams(0.2, 50) {}

void GlobalAlignState::createGui() {
  using Clouds = decltype(mApp.getScene().clouds);
  Clouds &clouds = mApp.getScene().clouds;

  if (ImGui::Begin("Global align")) {
    ImGui::Combo(
        "Reference", &mReference,
        [](void *cloudsPtr, int n) {
          const Clouds &clouds = *reinterpret_cast<Clouds *>(cloudsPtr);
          return clouds[static_cast<size_t>(n)].name.c_str();
        },
        reinterpret_cast<void *>(&clouds), clouds.size());

    ImGui::InputDouble("Voxel size", &mVoxelSize);
    ImGui::InputDouble("Normal search radius", &mNormalsParams.radius_);
    ImGui::InputInt("Normal search maximum nearest neighbors",
                    &mNormalsParams.max_nn_);
    if (ImGui::Button("Voxel down & compute normals")) {
      findNormals();
    }

    ImGui::InputDouble("Feature search radius", &mSearchParams.radius_);
    ImGui::InputInt("Feature search maximum nearest neighbors",
                    &mSearchParams.max_nn_);
    ImGui::BeginDisabled(mVoxelized.empty());
    if (ImGui::Button("Extract features")) {
      findFeatures();
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(mFeatures.size() != mIndices.size());
    if (ImGui::Button("Match features")) {
      matchFeatures();
    }
    ImGui::EndDisabled();

    ImGui::InputDouble("Refine voxel size", &mRefineVoxel);
    ImGui::InputDouble("Refine maximum distance", &mRefineThreshold);
    ImGui::BeginDisabled(mMatrices.size() != mIndices.size());
    if (ImGui::Button("Refine locally")) {
      refine();
    }
    ImGui::EndDisabled();

    if (ImGui::Button("Run")) {
      findNormals() && findFeatures() && matchFeatures() && refine();
    }

    ImGui::BeginDisabled(mMatrices.size() != mIndices.size());
    if (ImGui::Button("Apply")) {
      for (size_t i = 0; i < mIndices.size(); i++) {
        clouds[i].matrix = mMatrices[i];
        clouds[i].rawMatrix = true;
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      mApp.setState(std::make_unique<EditorState>(mApp));
    }
  }
  ImGui::End();
}

std::shared_ptr<open3d::geometry::PointCloud>
GlobalAlignState::voxelDown(size_t idx, double voxelSize,
                            std::optional<glm::mat4> m) const {
  const auto &clouds = mApp.getScene().clouds;
  auto pcd = clouds[idx].getPointCloud().VoxelDownSample(voxelSize);
  if (pcd) {
    if (!m) {
      m = clouds[idx].getMatrix();
    }
    pcd->Transform(
        Eigen::Map<const Eigen::Matrix4f>(glm::value_ptr(glm::inverse(*m)))
            .cast<double>());
  }
  return pcd;
}

bool GlobalAlignState::findNormals() {
  mVoxelized.clear();
  for (size_t idx : mIndices) {
    auto pcd = voxelDown(idx, mVoxelSize);
    if (!pcd) {
      mVoxelized.clear();
      return false;
    }
    pcd->EstimateNormals(mNormalsParams);
    mVoxelized.push_back(std::move(*pcd));
  }
  return true;
}

bool GlobalAlignState::findFeatures() {
  mFeatures.clear();
  for (const auto &pcd : mVoxelized) {
    auto feature = ComputeFPFHFeature(pcd, mSearchParams);
    if (!feature) {
      mFeatures.clear();
      return false;
    }
    mFeatures.push_back(std::move(*feature));
  }
  return true;
}

bool GlobalAlignState::matchFeatures() {
  mMatrices.clear();
  size_t ref = static_cast<size_t>(mReference);
  glm::mat4 refMatrix = mApp.getScene().clouds[ref].getMatrix();
  for (size_t i = 0; i < mVoxelized.size(); i++) {
    if (i == ref) {
      mMatrices.emplace_back(refMatrix);
      continue;
    }
    RegistrationResult res = FastGlobalRegistrationBasedOnFeatureMatching(
        mVoxelized[i], mVoxelized[ref], mFeatures[i], mFeatures[ref]);
    mMatrices.push_back(glm::mat4(glm::make_mat4(res.transformation_.data())) *
                        refMatrix);
  }
  return true;
}

bool GlobalAlignState::refine() {
  assert(mMatrices.size() == mIndices.size());

  size_t refIdx = static_cast<size_t>(mReference);
  auto ref = voxelDown(refIdx, mRefineVoxel);
  if (!ref) {
    return false;
  }

  for (size_t i = 0; i < mMatrices.size(); i++) {
    if (i == refIdx) {
      continue;
    }
    auto pcd = voxelDown(mIndices[i], mRefineVoxel, mMatrices[i]);
    if (!pcd) {
      return false;
    }
    RegistrationResult res = RegistrationICP(*pcd, *ref, mRefineThreshold);
    mMatrices[i] =
        glm::mat4(glm::make_mat4(res.transformation_.data())) * mMatrices[i];
  }
  return true;
}

void GlobalAlignState::render(const glm::mat4 &pv) {
  const auto &clouds = mApp.getScene().clouds;
  Renderer &r = mApp.getRenderer();
  r.beginRendering(pv);
  if (!mMatrices.empty()) {
    assert(mMatrices.size() == mIndices.size());
    for (size_t i = 0; i < mMatrices.size(); i++) {
      size_t idx = mIndices[i];
      r.renderPointCloud(idx, mMatrices[i], clouds[idx].color);
    }
  }
  r.endRendering();
}
