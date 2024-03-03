/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include "open3d/pipelines/registration/Registration.h"

#include "Application.h"

class AlignState : public AppState {
public:
  AlignState(Application &app, size_t reference, size_t toAlign);
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  void runIcp();
  void voxelDown();
  void estimateNormals();

  Application &mApp;

  size_t mReferenceIndex;
  size_t mAlignIndex;

  double mVoxelSize = 0.005;
  open3d::geometry::KDTreeSearchParamKNN mNormalsParam;

  std::shared_ptr<open3d::geometry::PointCloud> mReference;
  std::shared_ptr<open3d::geometry::PointCloud> mAlign;

  double mMaxDistance = 0.1;
  open3d::pipelines::registration::ICPConvergenceCriteria mCriteria;
  open3d::pipelines::registration::RegistrationResult mLastResult;
};
