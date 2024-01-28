/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include <memory>

#include "glad/glad.h"

#include "GLFW/glfw3.h"

#include "glm/glm.hpp"

#include "Scene.h"

class AppState {
public:
  virtual ~AppState() = default;
  virtual void createGui() {}
  virtual void render(const glm::mat4 &pv) { (void)pv; }
};

class Application {
public:
  enum class MouseMovement {
    None,
    Rotate,
    Pan,
  };

  Application();
  ~Application();
  int run(const char *dataDirectory);

  void setState(std::unique_ptr<AppState> newState);

  Scene &getScene();

private:
  void initGlfw();
  void initImgui();
  void terminate();

  void keyCallback(int key, int scancode, int action, int mods);
  void mouseClickCallback(int button, int action, int mods);
  void mousePosCallback(double x, double y);
  void mouseScrollCallback(double xoffset, double yoffset);
  static void glfwErrorCallback(int error, const char *description);

  GLFWwindow *mWindow = nullptr;
  bool mHasImgui = false;

  std::unique_ptr<AppState> mCurrentState;
  std::unique_ptr<AppState> mPendingState;
  std::unique_ptr<Scene> mScene;

  glm::mat4 mCamFrame;
  MouseMovement mMouseCaptured = MouseMovement::None;

  bool mImguiDemo = false;
};
