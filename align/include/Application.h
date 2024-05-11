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

#include "Renderer.h"
#include "Scene.h"

class AppState {
public:
  virtual ~AppState() = default;
  virtual void start() {}
  virtual void createGui() {}
  virtual void render(const glm::mat4 &pv) { (void)pv; }
  virtual bool keyCallback(int key, int scancode, int action, int mods) {
    (void)key;
    (void)scancode;
    (void)action;
    (void)mods;
    return false;
  }
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

  void setTitleDetails(const std::string &details);

  Renderer &getRenderer();
  Scene &getScene();
  const Scene &getScene() const;

  void refreshBuffer(std::optional<double> voxelSize = std::nullopt);
  void renderScene(const glm::mat4 &pv, bool paintUniform = false) const;

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

  // We need to defer the renderer initialization until we have loaded OpenGL.
  std::optional<Renderer> mRenderer;

  std::unique_ptr<AppState> mCurrentState;
  std::unique_ptr<AppState> mPendingState;
  std::unique_ptr<Scene> mScene;

  glm::mat4 mCamFrame;
  MouseMovement mMouseCaptured = MouseMovement::None;
  bool mPerspective = true;

  bool mImguiDemo = false;
};
