/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include "Application.h"

class NoiseRemovalState : public AppState {
public:
  NoiseRemovalState(Application &app, PointCloud &pcd);
  void start() override;
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  void resetInitial();
  void applyResults(std::shared_ptr<open3d::geometry::PointCloud> newCloud,
                    const std::vector<size_t> &inliers);
  bool exportMask() const;

  Application &mApp;
  PointCloud &mPcd;
  open3d::geometry::PointCloud mInitialCloud;
  std::vector<Eigen::Vector2<unsigned int>> mInitialPixels;
  std::vector<size_t> mOutliers;

  int mNumNeighbors = 16;
  double mSigmaRatio = 2.0;
  int mPointThreshold = 16;
  double mRadius = 0.05;
  bool mUseMask = true;
  bool mShowOutliers = true;
};
