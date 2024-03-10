/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <set>

#include "Application.h"

class MergeState : public AppState {
public:
  MergeState(Application &app, const std::set<size_t> &indices);
  void start() override;
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  Application &mApp;
  std::set<size_t> mIndices;

  double mLength = 2.0;
  int mResolution = 500;
  double mSdfTrunc = 0.04;
  Eigen::Vector3f mOrigin;

  int mRenderMode = 0;

  std::string mPcdFilename;
  std::string mMeshFilename;

  std::shared_ptr<open3d::geometry::PointCloud> mPointCloud;
  std::shared_ptr<open3d::geometry::TriangleMesh> mMesh;
};
