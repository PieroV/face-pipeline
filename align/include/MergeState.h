/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <optional>
#include <set>

#include "open3d/pipelines/integration/TSDFVolume.h"
#include "open3d/pipelines/registration/Registration.h"

#include "Application.h"

class MergeState : public AppState {
public:
  MergeState(Application &app, const std::set<size_t> &indices);
  void start() override;
  void createGui() override;
  void render(const glm::mat4 &pv) override;

private:
  enum VolumeType {
    VT_Uniform,
    VT_Scalable,
  };

  enum RenderMode {
    RM_PointCloud,
    RM_Mesh,
    RM_Wireframe,
    RM_Max,
  };

  void runMerge();
  void createVolume();
  void integrateFrame(size_t idx);
  void alignFrame(size_t idx);
  void updateGraphics();
  void integrateNext();

  void createExportGui();
  void createInteractiveGui();
  void createSymmetrizeGui();
  double runSymmetrizePass(glm::dmat4 &matrix);
  void runColormapOptimizer();

  Application &mApp;
  std::vector<size_t> mIndices;

  int mVolumeType = VT_Uniform;
  open3d::pipelines::integration::TSDFVolumeColorType mColorType =
      open3d::pipelines::integration::TSDFVolumeColorType::RGB8;
  double mLength = 2.0;
  int mResolution = 500;
  double mVoxelSize = 0.005;
  double mSdfTrunc = 0.04;
  // Shift by half of the default mLength for x and y.
  Eigen::Vector3f mOrigin{-1.0f, -1.0f, 0.0f};
  std::unique_ptr<open3d::pipelines::integration::TSDFVolume> mVolume;

  double mIcpDistance = 0.01;
  open3d::pipelines::registration::ICPConvergenceCriteria mIcpCriteria;
  double mIcpMinFitness = 0.5;
  double mIcpLastFitness = 0;
  bool mAlignBeforeMerge = false;

  bool mInteractiveMerge = false;
  size_t mInteractiveNextIdx = 0;
  bool mShowFitness = false;

  bool mShowSymmetrize = false;
  int mSymmIterations = 30;
  double mSymmIcpThreshold = 0.01;
  double mSymmAngleThreshold = 0.5;

  glm::mat4 mMatrix{1.0f};
  int mRenderMode = RM_Mesh;
  size_t mTempCloud = std::numeric_limits<size_t>::max();

  std::string mPcdFilename;
  std::string mMeshFilename;

  std::shared_ptr<open3d::geometry::PointCloud> mPointCloud;
  std::shared_ptr<open3d::geometry::TriangleMesh> mMesh;
};
