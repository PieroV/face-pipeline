/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "AddFrameState.h"

#include <unordered_set>

#include <cstdio>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "open3d/io/ImageIO.h"

#include "strnatcmp.h"

#include "EditorState.h"
#include "colormap.h"

namespace fs = std::filesystem;

namespace {
template <typename> struct FilenameComparer {
  static bool compare(const fs::path &a, const fs::path &b) { return a < b; }
};

template <> struct FilenameComparer<char> {
  static bool compare(const fs::path &a, const fs::path &b) {
    return strnatcasecmp(a.c_str(), b.c_str()) < 0;
  }
};

/*
// TODO: Enable and check this on Windows. Requires <shlwapi.h>
template <> struct FilenameComparer<wchar_t> {
  static bool compare(const fs::path &a, const fs::path &b) {
    return StrCmpLogicalW(a.c_str(), b.c_str()) < 0;
  }
};
*/
} // namespace

AddFrameState::AddFrameState(Application &app)
    : mApp(app), mFrames(&FilenameComparer<fs::path::value_type>::compare) {
  const open3d::camera::PinholeCameraIntrinsic &intr =
      app.getScene().getCameraIntrinsic();
  if (intr.width_ <= 0 || intr.height_ <= 0) {
    throw std::runtime_error(
        "Called with a scene without valid camera parameters.");
  }
  mWidth = intr.width_;
  mHeight = intr.height_;

  glGenTextures(1, &mTexture);
  glBindTexture(GL_TEXTURE_2D, mTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  listFrames();
}

AddFrameState::~AddFrameState() {
  glDeleteTextures(1, &mTexture);
  mTexture = 0;
}

void AddFrameState::listFrames() {
  Scene &scene = mApp.getScene();

  const fs::path &base = scene.getDataDirectory();
  const fs::path rgbPath = base / "rgb";
  const fs::path depthPath = base / "depth";
  // directory_iterator throws if the directory isn't valid, so we need to check
  // before calling it.
  if (!fs::exists(rgbPath) || !fs::is_directory(rgbPath) ||
      !fs::exists(depthPath) || !fs::is_directory(depthPath)) {
    return;
  }

  std::unordered_set<fs::path> alreadyUsed;
  for (const PointCloud &pcd : scene.clouds) {
    alreadyUsed.insert(pcd.name);
  }

  for (const fs::directory_entry &entry : fs::directory_iterator(rgbPath)) {
    const fs::path &p = entry.path();
    fs::path name = p.filename();
    // In the future we might allow png also for RGB data (and maybe do a
    // case-insensitive comparison).
    if (name.extension() != ".jpg") {
      continue;
    }
    name.replace_extension("");
    if (alreadyUsed.count(name)) {
      continue;
    }
    fs::path maybeDepth = depthPath / name;
    maybeDepth.replace_extension(".png");
    if (fs::exists(maybeDepth)) {
      mFrames.insert(name);
    }
  }

  mCurrentFrame = mFrames.begin();
  while (!mFrames.empty() && !updateTexture())
    ;
}

bool AddFrameState::updateTexture() {
  if (mFrames.empty() || mCurrentFrame == mFrames.end()) {
    return false;
  }
  const Scene &scene = mApp.getScene();
  open3d::geometry::Image colormap;
  try {
    if (*mCurrentFrame != mLastLoaded) {
      std::tie(mLastRgb, mLastDepth) = scene.openFrame(*mCurrentFrame);
    }
    colormap =
        createColormap(mLastRgb, mLastDepth, mBlend,
                       static_cast<float>(scene.getDepthScale()), mTrunc);
    mLastLoaded = *mCurrentFrame;
  } catch (std::exception &e) {
    fprintf(stderr, "%s\n", e.what());
    mCurrentFrame = mFrames.erase(mCurrentFrame);
    return false;
  }

  glBindTexture(GL_TEXTURE_2D, mTexture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_RGB, GL_FLOAT,
                  colormap.data_.data());
  return true;
}

void AddFrameState::createGui() {
  ImGui::Begin("Add frame");
  if (mFrames.empty()) {
    ImGui::Text("It isn't possible to add any frame.");
  } else {
    showFrame();
  }
  if (ImGui::Button("Close", ImVec2(120, 0))) {
    mApp.setState(std::make_unique<EditorState>(mApp));
  }
  ImGui::End();
}

void AddFrameState::showFrame() {
  assert(!mFrames.empty() && mCurrentFrame != mFrames.end());
  if (ImGui::SliderFloat("Blend", &mBlend, 0.0f, 1.0f)) {
    updateTexture();
  }
  if (ImGui::DragFloat("Truncate", &mTrunc, 0.01f, 0.0f, 20.0f)) {
    updateTexture();
  }
  // Sigh. This is the official way of showing an image with ImGui.
  // https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#example-for-opengl-users
  ImGui::Image((void *)(intptr_t)mTexture, ImVec2(mWidth, mHeight));

  std::string filename = mCurrentFrame->string();
  if (ImGui::InputText("Filename", &filename)) {
    fs::path current = *mCurrentFrame;
    FrameSet::const_iterator maybeNew = mFrames.find(filename);
    if (maybeNew != mFrames.end()) {
      mCurrentFrame = maybeNew;
      if (!updateTexture()) {
        // updateTexture deletes invalid entries, so find a new iterator.
        // The old iterator should be still valid according to the
        // documentation, but we prefer a safer approach and get a new one.
        mCurrentFrame = mFrames.find(current);
      }
    }
  }

  ImGui::PushButtonRepeat(true);
  if (ImGui::Button("Previous")) {
    do {
      if (mCurrentFrame == mFrames.begin()) {
        mCurrentFrame = mFrames.end();
      }
      // When updateTexture remove invalid frames, it forwards the iterator as
      // well, so we need to backward every time.
      mCurrentFrame--;
    } while (!updateTexture() && !mFrames.empty());
  }
  ImGui::SameLine();
  if (ImGui::Button("Next")) {
    // When updateTexture remove invalid frames, it already forwards the
    // iterator, so we do it only the first time.
    mCurrentFrame++;
    do {
      if (mCurrentFrame == mFrames.end()) {
        mCurrentFrame = mFrames.begin();
      }
    } while (!updateTexture() && !mFrames.empty());
  }
  ImGui::PopButtonRepeat();

  if (ImGui::Button("Add", ImVec2(120, 0))) {
    Scene &scene = mApp.getScene();
    scene.clouds.emplace_back(scene, mCurrentFrame->string(), mTrunc);
    scene.refreshBuffer();
    mCurrentFrame = mFrames.erase(mCurrentFrame);
    while (!mFrames.empty() && !updateTexture()) {
      if (mCurrentFrame == mFrames.end()) {
        mCurrentFrame = mFrames.begin();
      }
    }
    // Do not go back to the editor, to allow adding multiple frames at once.
  }
}
