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
  void mousePosCallback(double x, double y);
  static void glfwErrorCallback(int error, const char *description);

  void updateCamera(float dt);

  GLFWwindow *mWindow = nullptr;
  bool mHasImgui = false;

  std::unique_ptr<AppState> mCurrentState;
  std::unique_ptr<AppState> mPendingState;
  std::unique_ptr<Scene> mScene;

  glm::mat4 mView;
  glm::vec3 mCamPos;
  float mCamVert = 0.0f;
  float mCamHor = 0.0f;
  bool mMouseCaptured = false;
  double mMouseDX = 0;
  double mMouseDY = 0;

  bool mImguiDemo = false;
};
