/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <set>

#include "open3d/geometry/KDTreeSearchParam.h"
#include "open3d/pipelines/registration/Feature.h"
#include "open3d/pipelines/registration/Registration.h"

#include "Application.h"

class GlobalAlignState : public AppState {
public:
  GlobalAlignState(Application &app, const std::set<size_t> &indices);
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  std::shared_ptr<open3d::geometry::PointCloud>
  voxelDown(size_t idx, double voxelSize,
            std::optional<glm::mat4> m = std::nullopt) const;
  bool findNormals();
  bool findFeatures();
  bool matchFeatures();
  bool refine();

  Application &mApp;

  std::vector<size_t> mIndices;
  int mReference = 0;

  double mVoxelSize = 0.05;
  open3d::geometry::KDTreeSearchParamHybrid mNormalsParams;
  std::vector<open3d::geometry::PointCloud> mVoxelized;
  open3d::geometry::KDTreeSearchParamHybrid mSearchParams;
  std::vector<open3d::pipelines::registration::Feature> mFeatures;
  double mRefineVoxel = 0.005;
  double mRefineThreshold = 0.01;
  std::vector<glm::mat4> mMatrices;
};
