/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "AlignState.h"

#include <algorithm>

#include "glm/gtc/epsilon.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"

#include "EditorState.h"

using namespace open3d::pipelines::registration;

AlignState::AlignState(Application &app, size_t reference, size_t toAlign)
    : mApp(app), mReferenceIndex(reference), mAlignIndex(toAlign) {
  Scene &scene = app.getScene();
  scene.paintUniform = true;
  mReference = scene.clouds[reference].getPointCloudCopy();
  mAlign = scene.clouds[toAlign].getPointCloudCopy();
  assert(mReference && mAlign);
  estimateNormals();
}

void AlignState::createGui() {
  Scene &scene = mApp.getScene();
  PointCloud &ref = scene.clouds[mReferenceIndex];
  PointCloud &align = scene.clouds[mAlignIndex];

  ImGui::Begin("Align");

  ImGui::Text("Aligning: %zu - %s", mAlignIndex, align.name.c_str());
  ImGui::Text("Reference: %zu - %s", mReferenceIndex, ref.name.c_str());
  if (ImGui::Button("Swap")) {
    std::swap(mReferenceIndex, mAlignIndex);
    std::swap(mReference, mAlign);
  }

  ImGui::InputDouble("Voxel size", &mVoxelSize);
  ImGui::InputInt("Normals KNN", &mNormalsParam.knn_);
  ImGui::BeginDisabled(mVoxelSize <= 0.0 || mNormalsParam.knn_ <= 0);
  if (ImGui::Button("Voxel down")) {
    voxelDown();
  }
  ImGui::EndDisabled();

  ImGui::InputDouble("Maximum distance", &mMaxDistance);
  ImGui::InputInt("Maximum iterations", &mCriteria.max_iteration_);
  ImGui::InputDouble("Relative fitness", &mCriteria.relative_fitness_, 0.0, 0.0,
                     "%e");
  ImGui::InputDouble("Relative RMSE", &mCriteria.relative_rmse_, 0.0, 0.0,
                     "%e");

  ImGui::Text("Last RMSE: %f", mLastResult.inlier_rmse_);
  ImGui::Text("Last fitness: %f", mLastResult.fitness_);

  // Maybe we could check also the relative fitness/RMSE.
  ImGui::BeginDisabled(mMaxDistance <= 0 || mCriteria.max_iteration_ <= 0);
  if (ImGui::Button("Align")) {
    runIcp();
  }
  ImGui::EndDisabled();

  if (ImGui::Button("Close")) {
    mApp.setState(std::make_unique<EditorState>(mApp));
  }

  ImGui::End();
}

void AlignState::render(const glm::mat4 &pv) {
  // TODO: Make the renderer more generic, and render only the point cloud we
  // are working with.
  mApp.getScene().render(pv);
}

void AlignState::runIcp() {
  Scene &scene = mApp.getScene();
  PointCloud &ref = scene.clouds[mReferenceIndex];
  PointCloud &align = scene.clouds[mAlignIndex];

  assert(mReference && mAlign);

  glm::mat4 matRef = ref.getMatrix();
  glm::mat4 matAlign = align.getMatrix();

  // Theoretically, our 3x3 matrix is always a rotation, therefore orthonormal.
  // So, we could just transpose it to invert it:
  // glm::mat4 refInv(glm::transpose(glm::mat3(matRef)));
  // refInv[3] = -refInv * matRef[3];
  // refInv[3][3] = 1.0;
  // However, an assertion like assert(isIdentity(refInv * matRef)) sometimes
  // fails even with an epsilon one would expect to be big enough (e.g., 1e-5).
  // I'm not sure of the cause. It could be a float error, numerical stability,
  // or ICP not producing a pure rigid motion.
  // Anyway, we compute the inverse only when we need to run an alignment,
  // so it's fine to use the generic one. If this is so problematic, we could
  // even compute it once at save it as a member.
  glm::mat4 T = glm::inverse(matRef) * matAlign;
  // Both Eigen and GLM are column-major, we can just pass pointers.
  Eigen::Map<Eigen::Matrix4f> init(glm::value_ptr(T));
  // TODO: Should we add a UI element to choose the estimation method?
  RegistrationResult result =
      RegistrationICP(*mAlign, *mReference, mMaxDistance, init.cast<double>(),
                      TransformationEstimationPointToPlane(), mCriteria);
  // FIXME: Find a way to check if the matrix is valid, instead.
  if (result.fitness_ > 0.1) {
    mLastResult = result;
    T = glm::make_mat4(mLastResult.transformation_.data());
    // TODO: Save any changes temporarily, and give a way to go accept the
    // changes at the end or go back to the original matrix.
    align.matrix = matRef * T;
    align.rawMatrix = true;
  }
}

void AlignState::voxelDown() {
  const Scene &scene = mApp.getScene();
  mReference =
      scene.clouds[mReferenceIndex].getPointCloud().VoxelDownSample(mVoxelSize);
  mAlign =
      scene.clouds[mAlignIndex].getPointCloud().VoxelDownSample(mVoxelSize);
  if (!mReference || !mAlign) {
    // I don't expect this to actually happen, but this call depends on external
    // code, so it makes sense to throw, instead of asserting.
    throw std::runtime_error("Failed to down sample the point clouds.");
  }
  // I've observed estimating the normals after down sampling yielded us better
  // results than estimating them when we have a lot of data and then averaging
  // when down sampling.
  estimateNormals();
}

void AlignState::estimateNormals() {
  assert(mReference && mAlign);
  mReference->EstimateNormals(mNormalsParam);
  mAlign->EstimateNormals(mNormalsParam);
}
