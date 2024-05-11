/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "Application.h"

#include <cassert>
#include <cstdio>
#include <stdexcept>

#include "imgui.h"

#include "LoadState.h"

const char appTitle[] = "Aligner";

Application::Application() : BaseApplication(appTitle) {
  try {
    mRenderer.emplace();
  } catch (std::exception &e) {
    terminateBase();
    throw e;
  }
}

void Application::beginFrame() {
  if (mPendingState) {
    mCurrentState.reset();
    mCurrentState = std::move(mPendingState);
    mCurrentState->start();
  }
  assert(mCurrentState);
}

void Application::createGui() {
  assert(mCurrentState);
  mCurrentState->createGui();
}

void Application::render() {
  assert(mCurrentState);
  mCurrentState->render(mProjection * mView);
}

int Application::run(const char *dataDirectory) {
  mCurrentState = std::make_unique<LoadState>(*this, mScene, dataDirectory);
  mCurrentState->start();
  return BaseApplication::run();
}

void Application::keyCallback(int key, int scancode, int action, int mods) {
  if (ImGui::GetIO().WantCaptureKeyboard) {
    return;
  }
  if (mCurrentState &&
      mCurrentState->keyCallback(key, scancode, action, mods)) {
    return;
  }
  BaseApplication::keyCallback(key, scancode, action, mods);
}

void Application::setState(std::unique_ptr<AppState> newState) {
  if (!newState) {
    throw std::invalid_argument("The state cannot be null");
  }
  mPendingState = std::move(newState);
}

void Application::setTitleDetails(const std::string &details) {
  std::string title = appTitle;
  if (!details.empty()) {
    title += " — ";
    title += details;
  }
  glfwSetWindowTitle(mWindow, title.c_str());
}

Renderer &Application::getRenderer() {
  // Failing to initialize the renderer should close the application
  // immediately.
  assert(mRenderer);
  return *mRenderer;
}

Scene &Application::getScene() {
  if (!mScene) {
    throw std::runtime_error("Scene not available.");
  }
  return *mScene;
}

const Scene &Application::getScene() const {
  if (!mScene) {
    throw std::runtime_error("Scene not available.");
  }
  return *mScene;
}

void Application::refreshBuffer(std::optional<double> voxelSize) {
  assert(mRenderer);
  mRenderer->clearBuffer();
  const auto &clouds = getScene().clouds;
  for (const PointCloud &pcd : clouds) {
    mRenderer->addPointCloud(pcd, voxelSize);
  }
  mRenderer->uploadBuffer();
}

void Application::renderScene(const glm::mat4 &pv, bool paintUniform) const {
  assert(mRenderer);
  mRenderer->beginRendering(pv);
  const auto &clouds = getScene().clouds;
  for (size_t i = 0; i < clouds.size(); i++) {
    if (clouds[i].hidden) {
      continue;
    }
    std::optional<glm::vec3> color;
    if (paintUniform) {
      color = clouds[i].color;
    }
    mRenderer->renderPointCloud(i, clouds[i].matrix, color);
  }
  mRenderer->endRendering();
}
