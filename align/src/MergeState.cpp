/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "MergeState.h"

#include "glm/ext/quaternion_exponential.hpp"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "open3d/io/PointCloudIO.h"
#include "open3d/io/TriangleMeshIO.h"
#include "open3d/pipelines/color_map/RigidOptimizer.h"
#include "open3d/pipelines/integration/ScalableTSDFVolume.h"
#include "open3d/pipelines/integration/UniformTSDFVolume.h"

#include "EditorState.h"
#include "utilities.h"

MergeState::MergeState(Application &app, const std::set<size_t> &indices)
    : mApp(app), mIndices(indices.begin(), indices.end()) {
  if (indices.empty()) {
    throw std::invalid_argument("Indices cannot be empty.");
  }
}

void MergeState::start() {
  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
}

void MergeState::createGui() {
  if (ImGui::Begin("Merge")) {
    ImGui::BeginDisabled(mInteractiveMerge);

    ImGui::RadioButton("Uniform", &mVolumeType, VT_Uniform);
    ImGui::SameLine();
    ImGui::RadioButton("Scalable", &mVolumeType, VT_Scalable);

    if (mVolumeType == VT_Uniform) {
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
      ImGui::InputFloat3("Origin", mOrigin.data());
    } else if (mVolumeType == VT_Scalable) {
      ImGui::InputDouble("Voxel size", &mVoxelSize);
    }

    ImGui::InputDouble("SDF truncation value", &mSdfTrunc);
    ImGui::Checkbox("Align frames before merging", &mAlignBeforeMerge);
    ImGui::EndDisabled();

    ImGui::InputDouble("Align maximum distance", &mIcpDistance);
    ImGui::InputInt("Align maximum iterations", &mIcpCriteria.max_iteration_);
    ImGui::InputDouble("Align minimum fitness", &mIcpMinFitness);

    ImGui::BeginDisabled(mInteractiveMerge);
    if (ImGui::Button("Shuffle inputs")) {
      // We take for granted the first first frame is more or less the desired
      // final alignment, so we never move it.
      std::shuffle(mIndices.begin() + 1, mIndices.end(), getRng());
    }
    if (ImGui::Button("Merge")) {
      runMerge();
    }
    ImGui::SameLine();
    if (ImGui::Button("Interactive merge")) {
      mInteractiveMerge = true;
      mInteractiveNextIdx = 0;
      createVolume();
      integrateNext();
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(!mPointCloud || mInteractiveMerge);
    if (ImGui::Button("Align frames to the result")) {
      for (size_t idx : mIndices) {
        alignFrame(idx);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Symmetrize")) {
      mShowSymmetrize = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!mMesh || mInteractiveMerge);
    if (ImGui::Button("Optimize colormap")) {
      runColormapOptimizer();
    }
    ImGui::EndDisabled();

    createExportGui();

    static const char *symLabels[RM_Max] = {"Point cloud", "Mesh",
                                            "Mesh wireframe"};
    assert(mRenderMode < RM_Max);
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
  }
  ImGui::End();

  createInteractiveGui();
  createSymmetrizeGui();
}

void MergeState::runMerge() {
  assert(!mIndices.empty());
  createVolume();
  integrateFrame(mIndices[0]);
  for (size_t i = 1; i < mIndices.size(); i++) {
    if (mAlignBeforeMerge) {
      mPointCloud = mVolume->ExtractPointCloud();
      alignFrame(mIndices[i]);
    }
    integrateFrame(mIndices[i]);
  }
  updateGraphics();
  mVolume.reset();
}

void MergeState::createVolume() {
  switch (mVolumeType) {
  case VT_Uniform:
    mVolume =
        std::make_unique<open3d::pipelines::integration::UniformTSDFVolume>(
            mLength, mResolution, mSdfTrunc,
            open3d::pipelines::integration::TSDFVolumeColorType::RGB8,
            mOrigin.cast<double>());
    break;
  case VT_Scalable:
    mVolume =
        std::make_unique<open3d::pipelines::integration::ScalableTSDFVolume>(
            mVoxelSize, mSdfTrunc,
            open3d::pipelines::integration::TSDFVolumeColorType::RGB8);
    break;
  default:
    throw std::runtime_error("Unexpected volume type.");
  }
}

void MergeState::integrateFrame(size_t idx) {
  using namespace Eigen;
  const PointCloud &pcd = mApp.getScene().clouds.at(idx);
  Matrix4d matrix = pcd.getMatrixEigen().inverse().eval();
  auto maybeMasked = pcd.getMaskedRgbd();
  assert(mVolume);
  mVolume->Integrate(maybeMasked ? *maybeMasked : pcd.getRgbdImage(),
                     mApp.getScene().getCameraIntrinsic(), matrix);
}

void MergeState::alignFrame(size_t idx) {
  using namespace Eigen;
  using namespace open3d::pipelines::registration;
  auto &clouds = mApp.getScene().clouds;
  assert(mPointCloud && idx < clouds.size());
  PointCloud &pcd = clouds[idx];
  Matrix4d init = pcd.getMatrixEigen();
  RegistrationResult res = RegistrationICP(
      pcd.getMaskedPointCloud(), *mPointCloud, mIcpDistance, init,
      TransformationEstimationPointToPlane(), mIcpCriteria);
  mIcpLastFitness = res.fitness_;
  if (mIcpLastFitness >= mIcpMinFitness) {
    pcd.matrix = glm::make_mat4(res.transformation_.data());
  }
}

void MergeState::updateGraphics() {
  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
  if (mVolume) {
    mPointCloud = mVolume->ExtractPointCloud();
    mMesh = mVolume->ExtractTriangleMesh();
  }
  if (mPointCloud && mMesh) {
    r.addPointCloud(*mPointCloud);
    r.addTriangleMesh(*mMesh);
  }
  if (mInteractiveMerge && mInteractiveNextIdx < mIndices.size()) {
    const auto &clouds = mApp.getScene().clouds;
    assert(mIndices[mInteractiveNextIdx] < clouds.size());
    mTempCloud = r.addPointCloud(
        clouds[mIndices[mInteractiveNextIdx]].getMaskedPointCloud());
  } else {
    mTempCloud = std::numeric_limits<size_t>::max();
  }
  r.uploadBuffer();
}

void MergeState::integrateNext() {
  assert(mVolume);
  integrateFrame(mIndices[mInteractiveNextIdx++]);
  mPointCloud = mVolume->ExtractPointCloud();
  mMesh = mVolume->ExtractTriangleMesh();
  updateGraphics();
}

void MergeState::createExportGui() {
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
}

void MergeState::createInteractiveGui() {
  if (!mInteractiveMerge) {
    return;
  }
  if (ImGui::Begin("Interactive merge", &mInteractiveMerge)) {
    ImGui::Text("Merged %zu/%zu", mInteractiveNextIdx, mIndices.size());

    bool hasNext = mInteractiveNextIdx < mIndices.size();
    if (hasNext) {
      ImGui::Text(
          "Showing: %s",
          mApp.getScene().clouds[mIndices[mInteractiveNextIdx]].name.c_str());
    } else {
      ImGui::Text("All frames integrated");
    }

    ImGui::BeginDisabled(!hasNext || !mPointCloud);
    if (ImGui::Button("Align")) {
      alignFrame(mIndices[mInteractiveNextIdx]);
      mShowFitness = true;
    }
    ImGui::EndDisabled();
    if (mShowFitness) {
      ImGui::Text("Alignment fitness: %f", mIcpLastFitness);
    }

    ImGui::BeginDisabled(!hasNext);
    if (ImGui::Button("Merge")) {
      integrateNext();
      mShowFitness = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Skip")) {
      mInteractiveNextIdx++;
      updateGraphics();
      mShowFitness = false;
    }
    ImGui::EndDisabled();

    if (ImGui::Button("Close")) {
      mInteractiveMerge = false;
    }
  }
  ImGui::End();
  if (!mInteractiveMerge) {
    mVolume.reset();
    mShowFitness = false;
  }
}

void MergeState::createSymmetrizeGui() {
  if (!mPointCloud) {
    mShowSymmetrize = false;
  }
  if (!mShowSymmetrize) {
    return;
  }

  if (!ImGui::Begin("Symmetrize", &mShowSymmetrize)) {
    ImGui::End();
    return;
  }

  ImGui::InputDouble("ICP threshold", &mSymmIcpThreshold);
  ImGui::InputDouble("Angle threshold", &mSymmAngleThreshold);
  ImGui::InputInt("Maximum iterations", &mSymmIterations);
  if (ImGui::Button("Run")) {
    glm::dmat4 matrix = mMatrix;
    for (int i = 0; i < mSymmIterations; i++) {
      double angle = runSymmetrizePass(matrix);
      if (angle < mSymmAngleThreshold) {
        break;
      }
    }
    mMatrix = matrix;
  }
  if (ImGui::Button("Accept & merge again")) {
    auto &clouds = mApp.getScene().clouds;
    for (size_t idx : mIndices) {
      assert(idx < clouds.size());
      clouds[idx].matrix = mMatrix * clouds[idx].matrix;
    }
    runMerge();
    mShowSymmetrize = false;
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    mShowSymmetrize = false;
  }
  if (!mShowSymmetrize) {
    mMatrix = glm::mat4(1.0f);
  }

  ImGui::End();
}

double MergeState::runSymmetrizePass(glm::dmat4 &matrix) {
  using namespace Eigen;
  using namespace open3d::pipelines::registration;
  assert(mPointCloud);

  open3d::geometry::PointCloud pcdNegative, pcdMirrored;
  for (const auto &vec : mPointCloud->points_) {
    glm::dvec4 hvec = matrix * glm::vec4(vec[0], vec[1], vec[2], 1.0);
    Vector3d tvec;
    tvec << hvec.x, hvec.y, hvec.z;
    if (tvec[0] < 0) {
      pcdNegative.points_.push_back(tvec);
    } else {
      tvec[0] *= -1;
      pcdMirrored.points_.push_back(tvec);
    }
  }

  RegistrationResult res =
      RegistrationICP(pcdMirrored, pcdNegative, mSymmIcpThreshold);
  glm::dmat4 transformation =
      glm::make_mat4<double>(res.transformation_.data());
  glm::dquat q = glm::quat_cast(transformation);
  double angle = glm::degrees(glm::angle(q));
  // https://math.stackexchange.com/a/162892
  q = glm::pow(q, -0.5);
  glm::dvec4 translation = transformation[3] * -0.5;
  translation.w = 1.0;
  transformation = glm::mat4_cast(q);
  transformation[3] = translation;
  matrix = transformation * matrix;
  return angle;
}

void MergeState::runColormapOptimizer() {
  using namespace Eigen;
  using namespace open3d;
  assert(mMesh);
  camera::PinholeCameraTrajectory trajectory;
  trajectory.parameters_.resize(mIndices.size());
  std::vector<geometry::RGBDImage> images;
  camera::PinholeCameraIntrinsic intr = mApp.getScene().getCameraIntrinsic();
  const auto &clouds = mApp.getScene().clouds;
  for (size_t i = 0; i < mIndices.size(); i++) {
    const PointCloud &pcd = clouds[mIndices[i]];
    images.push_back(pcd.hasMaskedRgbd() ? *pcd.getMaskedRgbd()
                                         : pcd.getRgbdImage());
    camera::PinholeCameraParameters &params = trajectory.parameters_[i];
    params.intrinsic_ = intr;
    params.extrinsic_ = pcd.getMatrixEigen().inverse().eval();
  }
  pipelines::color_map::RigidOptimizerOption options;
  auto res = pipelines::color_map::RunRigidOptimizer(*mMesh, images, trajectory,
                                                     options);
  mMesh = std::make_shared<geometry::TriangleMesh>(std::move(res.first));
  updateGraphics();
}

void MergeState::render(const glm::mat4 &pv) {
  Renderer &r = mApp.getRenderer();
  r.beginRendering(pv);
  if (mMesh && (mRenderMode == RM_Mesh || mRenderMode == RM_Wireframe)) {
    GLint polygonMode;
    glGetIntegerv(GL_POLYGON_MODE, &polygonMode);
    if (mRenderMode == RM_Wireframe) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    r.renderIndexedMesh(1, mMatrix);
    glPolygonMode(GL_FRONT_AND_BACK, polygonMode);
  } else if (mPointCloud) {
    r.renderPointCloud(0, mMatrix);
  }

  const auto &clouds = mApp.getScene().clouds;
  if (mInteractiveMerge && mInteractiveNextIdx < mIndices.size()) {
    assert(mTempCloud != std::numeric_limits<size_t>::max());
    const PointCloud &cloud = clouds[mIndices[mInteractiveNextIdx]];
    r.renderPointCloud(mTempCloud, cloud.matrix, cloud.color);
  }

  r.endRendering();
}
