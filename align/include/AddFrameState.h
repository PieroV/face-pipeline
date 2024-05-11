/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include "Application.h"

#include <functional>
#include <set>
#include <string>
#include <unordered_set>

class AddFrameState : public AppState {
public:
  using FrameSet =
      std::set<std::filesystem::path, bool (*)(const std::filesystem::path &a,
                                               const std::filesystem::path &b)>;

  AddFrameState(Application &app);
  ~AddFrameState();
  AddFrameState(const AddFrameState &other) = delete;
  AddFrameState &operator=(const AddFrameState &other) = delete;
  // We can implement the texture swap/creation if needed
  AddFrameState(AddFrameState &&other) = delete;
  AddFrameState &operator=(AddFrameState &&other) = delete;
  void createGui() override;
  void render(const glm::mat4 &pv) override;
  bool keyCallback(int key, int scancode, int action, int mods) override;

private:
  void listFrames();
  bool updateTexture();
  void showFrame();
  void prevFrame();
  void nextFrame();

  Application &mApp;
  FrameSet mFrames;
  FrameSet::const_iterator mCurrentFrame;
  std::unordered_set<std::filesystem::path> mAlreadyUsed;

  GLuint mTexture = 0;
  int mWidth;
  int mHeight;

  std::filesystem::path mLastLoaded;
  open3d::geometry::Image mLastRgb;
  open3d::geometry::Image mLastDepth;
  float mBlend = 0.5f;
  float mTrunc = 1.5f;
};
