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
  struct FramePair {
    FramePair() = default;
    FramePair(const std::filesystem::path &rgb, const std::filesystem::path &d);
    bool operator==(const FramePair &other) const;
    bool operator==(const std::string &depthStem) const;
    bool operator!=(const std::string &depthStem) const;
    bool operator<(const FramePair &other) const;
    bool operator<(const std::string &depthStem) const;
    std::string rgb;
    std::string d;
    std::string stem;
  };
  // Use std::less<> instead of std::less<Key> to enable comparison with
  // strings. See https://en.cppreference.com/w/cpp/container/set/find.
  using FrameSet = std::set<FramePair, std::less<>>;

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

  std::string mLastLoaded;
  open3d::geometry::Image mLastRgb;
  open3d::geometry::Image mLastDepth;
  float mBlend = 0.5f;
  float mTrunc = 1.5f;
};

bool operator<(const std::string &depthStem,
               const AddFrameState::FramePair &fp);
