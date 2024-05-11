/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#pragma once

#include "glad/glad.h"

#include "GLFW/glfw3.h"

#include "glm/glm.hpp"

class BaseApplication {
public:
  enum class MouseMovement {
    None,
    Rotate,
    Pan,
  };

  BaseApplication(const char *windowTitle);
  virtual ~BaseApplication();
  virtual int run();

protected:
  void initGlfw(const char *windowTitle);
  void initImgui();
  void terminateBase();

  virtual void beginFrame() {}
  virtual void createGui() {}
  virtual void render() {}

  virtual void keyCallback(int key, int scancode, int action, int mods);
  virtual void mouseClickCallback(int button, int action, int mods);
  virtual void mousePosCallback(double x, double y);
  virtual void mouseScrollCallback(double xoffset, double yoffset);

  GLFWwindow *mWindow = nullptr;
  bool mHasImgui = false;

  glm::mat4 mCamFrame{1.0f};
  glm::mat4 mView{0.0f};
  glm::mat4 mProjection{0.0f};
  MouseMovement mMouseCaptured = MouseMovement::None;

  bool mImguiDemo = false;
};
