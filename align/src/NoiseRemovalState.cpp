/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "NoiseRemovalState.h"

#include <unordered_set>

#include "imgui.h"

#include "open3d/io/ImageIO.h"

#include "EditorState.h"

NoiseRemovalState::NoiseRemovalState(Application &app, PointCloud &pcd)
    : mApp(app), mPcd(pcd) {}

void NoiseRemovalState::start() { resetInitial(); }

void NoiseRemovalState::createGui() {
  if (ImGui::Begin("Noise removal")) {
    ImGui::Text("Working on %s.", mPcd.name.c_str());
    ImGui::InputInt("Number of neighbors", &mNumNeighbors);
    if (mNumNeighbors < 0) {
      mNumNeighbors = 0;
    }
    ImGui::InputDouble("Sigma ratio", &mSigmaRatio);
    if (mSigmaRatio < 0) {
      mSigmaRatio = 0;
    }
    ImGui::BeginDisabled(mNumNeighbors <= 0 || mSigmaRatio <= 0);
    if (ImGui::Button("Remove noise (statistical)")) {
      auto [newCloud, inliers] = mInitialCloud.RemoveStatisticalOutliers(
          static_cast<size_t>(mNumNeighbors), mSigmaRatio);
      applyResults(newCloud, inliers);
    }
    ImGui::EndDisabled();
    ImGui::InputInt("Minimum number of points", &mPointThreshold);
    if (mPointThreshold < 0) {
      mPointThreshold = 0;
    }
    ImGui::InputDouble("Search radius", &mRadius);
    if (mRadius < 0) {
      mRadius = 0;
    }
    ImGui::BeginDisabled(mPointThreshold <= 0 || mRadius <= 0);
    if (ImGui::Button("Remove noise (radius)")) {
      auto [newCloud, inliers] = mInitialCloud.RemoveRadiusOutliers(
          static_cast<size_t>(mPointThreshold), mRadius);
      applyResults(newCloud, inliers);
    }
    ImGui::EndDisabled();
    if (ImGui::Checkbox("Use mask if available", &mUseMask)) {
      resetInitial();
    }
    ImGui::Checkbox("Show outliers", &mShowOutliers);
    ImGui::BeginDisabled(mOutliers.empty());
    if (ImGui::Button("Export mask")) {
      if (exportMask()) {
        mPcd.loadData(mApp.getScene());
        mApp.setState(std::make_unique<EditorState>(mApp));
      }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
      resetInitial();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      mApp.setState(std::make_unique<EditorState>(mApp));
    }
  }
  ImGui::End();
}

void NoiseRemovalState::resetInitial() {
  auto [points, pixels] = mApp.getScene().unprojectDepth(mPcd, mUseMask);
  mInitialCloud.points_ = std::move(points);
  mInitialCloud.PaintUniformColor(Eigen::Vector3d(0.0, 0.7, 0.0));
  mInitialPixels = std::move(pixels);
  mOutliers.clear();

  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
  r.addPointCloud(mInitialCloud);
  r.uploadBuffer();
}

void NoiseRemovalState::applyResults(
    std::shared_ptr<open3d::geometry::PointCloud> newCloud,
    const std::vector<size_t> &inliers) {
  if (!newCloud) {
    return;
  }

  size_t total = mInitialCloud.points_.size();
  size_t numOutliers = total - inliers.size();
  std::unordered_set<size_t> inlierSet(inliers.begin(), inliers.end());
  open3d::geometry::PointCloud outliers;
  outliers.points_.reserve(numOutliers);
  mOutliers.clear();
  mOutliers.reserve(numOutliers);
  for (size_t i = 0; i < total; i++) {
    if (!inlierSet.count(i)) {
      outliers.points_.push_back(mInitialCloud.points_[i]);
      mOutliers.push_back(i);
    }
  }
  outliers.PaintUniformColor(Eigen::Vector3d(1.0, 0.0, 0.0));

  Renderer &r = mApp.getRenderer();
  r.clearBuffer();
  r.addPointCloud(*newCloud);
  r.addPointCloud(outliers);
  r.uploadBuffer();
}

bool NoiseRemovalState::exportMask() const {
  namespace fs = std::filesystem;

  if (mOutliers.empty()) {
    return false;
  }

  open3d::geometry::Image mask;

  fs::path maskPath = mApp.getScene().getDataDirectory();
  maskPath /= "mask";
  maskPath /= mPcd.name + ".png";
  std::string maskPathStr = maskPath.string();
  std::error_code ec;
  if (std::filesystem::exists(maskPath, ec)) {
    open3d::io::ReadImage(maskPathStr, mask);
  }

  if (mask.IsEmpty()) {
    mask = mPcd.getRgbdImage().color_;
  } else if (mask.bytes_per_channel_ != 1) {
    return false;
  }
  if (mask.num_of_channels_ == 3) {
    // This will override any existing mask! We should warn about this...
    assert(mask.bytes_per_channel_ == 1 && mask.height_ > 0);
    using namespace Eigen;
    std::vector<uint8_t> origData = std::move(mask.data_);
    mask.num_of_channels_ = 4;
    mask.data_.resize(static_cast<size_t>(mask.height_) * mask.BytesPerLine());
    Map<const Matrix<uint8_t, Dynamic, 3, RowMajor>> origMat(
        origData.data(), mask.height_ * mask.width_, 3);
    Map<Matrix<uint8_t, Dynamic, 4, RowMajor>> maskMat(mask.data_.data(),
                                                       origMat.rows(), 4);
    maskMat << origMat, Vector<uint8_t, Dynamic>::Constant(origMat.rows(), 255);
  } else if (mask.num_of_channels_ != 4) {
    return false;
  }

  assert(mask.BytesPerLine() > 0 && mask.num_of_channels_ == 4);
  unsigned int stride = static_cast<unsigned int>(mask.BytesPerLine());
  for (size_t out : mOutliers) {
    assert(out < mInitialPixels.size());
    auto coords = mInitialPixels[out];
    size_t offset = coords[1] * stride + 4 * coords[0];
    mask.data_[offset + 3] = 0;
  }

  return open3d::io::WriteImageToPNG(maskPathStr, mask);
}

void NoiseRemovalState::render(const glm::mat4 &pv) {
  Renderer &r = mApp.getRenderer();
  r.beginRendering(pv);
  r.renderPointCloud(0);
  if (!mOutliers.empty() && mShowOutliers) {
    r.renderPointCloud(1);
  }
  r.endRendering();
}
