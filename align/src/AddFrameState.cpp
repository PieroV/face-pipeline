/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "AddFrameState.h"

#include <cstdio>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "open3d/io/ImageIO.h"

#include "strnatcmp.h"

#include "EditorState.h"
#include "colormap.h"

namespace fs = std::filesystem;

/**
 * Returns the extension of a path converted to lowercase.
 *
 * For our purposes, we accept only ASCII extensions, which simplifies this
 * function.
 */
static std::string lowercaseExtension(const fs::path &p);

AddFrameState::AddFrameState(Application &app) : mApp(app) {
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

  for (const PointCloud &pcd : scene.clouds) {
    mAlreadyUsed.insert(pcd.name);
  }

  // The following steps should not hurt a healthy dataset: we enable
  // case-insensitivity on the extension (and maybe also on the stem, in the
  // future), which however means that some frames might be discarded (and the
  // one that will be taken depends on the order on which we iterate).
  std::unordered_map<fs::path, fs::path> depthStems;
  for (const fs::directory_entry &entry : fs::directory_iterator(depthPath)) {
    fs::path p = entry.path().lexically_proximate(base);
    if (lowercaseExtension(p) != ".png") {
      continue;
    }
    auto res = depthStems.insert(std::make_pair(p.stem(), p));
    // In debug mode, assert about unhealthy datasets.
    assert(res.second && "Duplicated depth stem.");
  }

  // We support mulitple extensions (jpg, png and various variants in
  // case-sensitive filesystems), so multiple color frames could be associated
  // to the same depth frame.
  // However, the FrameSet class checks only the depth name (which has already
  // been forced to be unique), so only the first entry will be taken into
  // account. However, which one will be taken depends on the order of
  // iteration.
  // This should not be a problem for healthy dataset, but not a completely
  // correct solution either.
  for (const fs::directory_entry &entry : fs::directory_iterator(rgbPath)) {
    fs::path p = entry.path().lexically_proximate(base);
    std::string ext = lowercaseExtension(p);
    if (ext != ".jpg" && ext != ".png") {
      continue;
    }
    auto maybeDepth = depthStems.find(p.stem());
    if (maybeDepth != depthStems.end()) {
      mFrames.emplace(p, maybeDepth->second);
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
      std::tie(mLastRgb, mLastDepth) =
          scene.openFrame(mCurrentFrame->rgb, mCurrentFrame->d);
    }
    colormap =
        createColormap(mLastRgb, mLastDepth, mBlend,
                       static_cast<float>(scene.getDepthScale()), mTrunc);
    mLastLoaded = mCurrentFrame->stem;
  } catch (std::exception &e) {
    fprintf(stderr, "Cannot load %s: %s\n", mCurrentFrame->stem.c_str(),
            e.what());
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

void AddFrameState::render(const glm::mat4 &pv) { mApp.renderScene(pv); }

bool AddFrameState::keyCallback(int key, int scancode, int action, int mods) {
  (void)scancode;
  (void)mods;
  // Handle also GLFW_REPEAT.
  if (key == GLFW_KEY_LEFT && action != GLFW_RELEASE) {
    prevFrame();
    return true;
  } else if (key == GLFW_KEY_RIGHT && action != GLFW_RELEASE) {
    nextFrame();
    return true;
  }
  return false;
}

void AddFrameState::showFrame() {
  assert(!mFrames.empty() && mCurrentFrame != mFrames.end());
  if (ImGui::SliderFloat("Blend", &mBlend, 0.0f, 1.0f)) {
    updateTexture();
  }
  if (ImGui::DragFloat("Truncate", &mTrunc, 0.01f, 0.0f, 20.0f)) {
    updateTexture();
  }
  float width = std::max<float>(mWidth, ImGui::GetWindowWidth());
  float ratio = static_cast<float>(mHeight) / mWidth;
  float height = width * ratio;
  // Sigh. This is the official way of showing an image with ImGui.
  // https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#example-for-opengl-users
  ImGui::Image((void *)(intptr_t)mTexture, ImVec2(width, height));

  std::string filename = mCurrentFrame->stem;
  if (ImGui::InputText("Filename", &filename)) {
    const std::string &current = mCurrentFrame->stem;
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
    prevFrame();
  }
  ImGui::SameLine();
  if (ImGui::Button("Next")) {
    nextFrame();
  }
  ImGui::PopButtonRepeat();

  bool inScene = mAlreadyUsed.count(mCurrentFrame->stem);
  ImGui::BeginDisabled(inScene);
  if (ImGui::Button(inScene ? "Already added" : "Add", ImVec2(120, 0))) {
    Scene &scene = mApp.getScene();
    scene.clouds.emplace_back(scene, mCurrentFrame->stem, mCurrentFrame->rgb,
                              mCurrentFrame->d, mTrunc);
    mAlreadyUsed.insert(mCurrentFrame->stem);
    mApp.refreshBuffer();
    mCurrentFrame = mFrames.erase(mCurrentFrame);
    while (!mFrames.empty() && !updateTexture()) {
      if (mCurrentFrame == mFrames.end()) {
        mCurrentFrame = mFrames.begin();
      }
    }
    // Do not go back to the editor, to allow adding multiple frames at once.
  }
  ImGui::EndDisabled();
}

void AddFrameState::prevFrame() {
  do {
    if (mCurrentFrame == mFrames.begin()) {
      mCurrentFrame = mFrames.end();
    }
    // When updateTexture remove invalid frames, it forwards the iterator as
    // well, so we need to backward every time.
    mCurrentFrame--;
  } while (!updateTexture() && !mFrames.empty());
}

void AddFrameState::nextFrame() {
  // When updateTexture remove invalid frames, it already forwards the
  // iterator, so we do it only the first time.
  mCurrentFrame++;
  do {
    if (mCurrentFrame == mFrames.end()) {
      mCurrentFrame = mFrames.begin();
    }
  } while (!updateTexture() && !mFrames.empty());
}

AddFrameState::FramePair::FramePair(const fs::path &rgb, const fs::path &d_)
    : rgb(rgb.string()), d(d_.string()), stem(d_.stem().string()) {
  if (stem.empty()) {
    throw std::invalid_argument("The depth's stem cannot be empty.");
  }
  if (lowercaseExtension(d_) != ".png") {
    throw std::invalid_argument("The depth should have .png extension.");
  }
}

bool AddFrameState::FramePair::operator==(const FramePair &other) const {
  return stem == other.stem;
}

bool AddFrameState::FramePair::operator==(const std::string &depthStem) const {
  return stem == depthStem;
}

bool AddFrameState::FramePair::operator!=(const std::string &depthStem) const {
  return stem != depthStem;
}

bool AddFrameState::FramePair::operator<(const FramePair &other) const {
  return *this < other.stem;
}

bool AddFrameState::FramePair::operator<(const std::string &depthStem) const {
  return strnatcasecmp(stem.c_str(), depthStem.c_str()) < 0;
}

bool operator==(const std::string &depthStem,
                const AddFrameState::FramePair &fp) {
  return fp == depthStem;
}

bool operator<(const std::string &depthStem,
               const AddFrameState::FramePair &fp) {
  return strnatcasecmp(depthStem.c_str(), fp.stem.c_str()) < 0;
}

static std::string lowercaseExtension(const fs::path &p) {
  fs::path ext = p.extension();
  std::string ret;
  ret.reserve(ext.native().length());
  for (auto ch : ext.native()) {
    // No Unicode and no surrogates, since we are ASCII only.
    if (ch > 0x7F) {
      return "";
    }
    // tolower should be fine with ASCII-only.
    ret.push_back(static_cast<char>(tolower(static_cast<unsigned char>(ch))));
  }
  return ret;
}
