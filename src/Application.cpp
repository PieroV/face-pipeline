/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "Application.h"

#include <cassert>
#include <cstdio>
#include <stdexcept>

#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h"

#include "LoadState.h"

static Application *appFromWindow(GLFWwindow *window);

Application::Application() : mCamFrame(1.0f) {
  glfwSetErrorCallback(glfwErrorCallback);

  std::exception_ptr eptr;
  try {
    initGlfw();
    initImgui();
  } catch (...) {
    eptr = std::current_exception();
  }
  if (eptr) {
    terminate();
    std::rethrow_exception(eptr);
  }
}

Application::~Application() { terminate(); }

void Application::initGlfw() {
  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW.");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  mWindow = glfwCreateWindow(1280, 720, "Aligner", nullptr, nullptr);
  if (!mWindow) {
    throw std::runtime_error("Failed to create a GLFW window");
  }
  glfwSetWindowUserPointer(mWindow, this);

  glfwMakeContextCurrent(mWindow);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    throw std::runtime_error("Failed to initialize GLAD");
  }
  glfwSetKeyCallback(mWindow, [](GLFWwindow *window, int key, int scancode,
                                 int action, int mods) {
    if (Application *app = appFromWindow(window)) {
      app->keyCallback(key, scancode, action, mods);
    }
  });
  glfwSetMouseButtonCallback(
      mWindow, [](GLFWwindow *window, int button, int action, int mods) {
        if (Application *app = appFromWindow(window)) {
          app->mouseClickCallback(button, action, mods);
        }
      });
  glfwSetCursorPosCallback(mWindow, [](GLFWwindow *window, double x, double y) {
    if (Application *app = appFromWindow(window)) {
      app->mousePosCallback(x, y);
    }
  });
  glfwSetScrollCallback(mWindow,
                        [](GLFWwindow *window, double xoffset, double yoffset) {
                          if (Application *app = appFromWindow(window)) {
                            app->mouseScrollCallback(xoffset, yoffset);
                          }
                        });
}

void Application::glfwErrorCallback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static Application *appFromWindow(GLFWwindow *window) {
  return static_cast<Application *>(glfwGetWindowUserPointer(window));
}

void Application::initImgui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  ImGui::StyleColorsLight();
  ImGui_ImplGlfw_InitForOpenGL(mWindow, true);
  const char *glslVersion = "#version 330 core";
  ImGui_ImplOpenGL3_Init(glslVersion);
  mHasImgui = true;
}

void Application::terminate() {
  if (mHasImgui) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    mHasImgui = false;
  }
  if (mWindow) {
    glfwDestroyWindow(mWindow);
    mWindow = nullptr;
  }
  // According to the documentation, calling it even if the initialization
  // failed is not a problem.
  glfwTerminate();
}

int Application::run(const char *dataDirectory) {
  mCurrentState = std::make_unique<LoadState>(*this, mScene, dataDirectory);

  ImGuiIO &io = ImGui::GetIO();

  while (!glfwWindowShouldClose(mWindow)) {
    if (mPendingState) {
      mCurrentState.reset();
      mCurrentState = std::move(mPendingState);
    }

    assert(mCurrentState);

    glfwPollEvents();
    if (mMouseCaptured != MouseMovement::None) {
      io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
      io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    mCurrentState->createGui();

    if (mImguiDemo) {
      ImGui::ShowDemoWindow(&mImguiDemo);
    }

    ImGui::Render();

    int w = 0, h = 0;
    glfwGetFramebufferSize(mWindow, &w, &h);
    glViewport(0, 0, w, h);

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)w / (float)h,
                                      0.1f, 100.0f);
    // Eye at center simplifies the rotation.
    glm::mat4 view = mCamFrame * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),
                                             glm::vec3(0.0f, 2.0f, 0.0f),
                                             glm::vec3(0.0f, 0.0f, 1.0f));
    mCurrentState->render(proj * view);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(mWindow);
  }

  return 0;
}

void Application::keyCallback(int key, int scancode, int action, int mods) {
  if (mCurrentState &&
      mCurrentState->keyCallback(key, scancode, action, mods)) {
    return;
  }
  if (key == GLFW_KEY_F10 && action == GLFW_PRESS) {
    mImguiDemo = !mImguiDemo;
    return;
  }
  if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    mCamFrame = glm::mat4(1.0f);
  }
}

void Application::mouseClickCallback(int button, int action, int mods) {
  (void)mods;
  if (ImGui::GetIO().WantCaptureMouse) {
    return;
  }
  MouseMovement mov;
  switch (button) {
  case GLFW_MOUSE_BUTTON_1:
    mov = MouseMovement::Rotate;
    break;
  case GLFW_MOUSE_BUTTON_2:
    mov = MouseMovement::Pan;
    break;
  default:
    return;
  }
  if (action == GLFW_RELEASE && mov == mMouseCaptured) {
    mMouseCaptured = MouseMovement::None;
    glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  } else if (mMouseCaptured == MouseMovement::None && action == GLFW_PRESS) {
    mMouseCaptured = mov;
    glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    int w = 0, h = 0;
    glfwGetWindowSize(mWindow, &w, &h);
    glfwSetCursorPos(mWindow, w / 2, h / 2);
  }
}

void Application::mousePosCallback(double x, double y) {
  if (mMouseCaptured == MouseMovement::None) {
    return;
  }

  int cx = 0, cy = 0;
  glfwGetWindowSize(mWindow, &cx, &cy);
  cx /= 2;
  cy /= 2;
  glfwSetCursorPos(mWindow, cx, cy);
  float dx = static_cast<float>(x - cx);
  float dy = static_cast<float>(y - cy);

  switch (mMouseCaptured) {
  case MouseMovement::Rotate: {
    const float sensitivity = 0.001f;
    glm::mat4 newFrame = glm::eulerAngleXY(dy * sensitivity, dx * sensitivity) *
                         glm::mat4(glm::mat3(mCamFrame));
    newFrame[3] = mCamFrame[3];
    mCamFrame = newFrame;
    break;
  }

  case MouseMovement::Pan: {
    const float sensitivity = 0.005f;
    // Our frame is in view space, Y is now the vertical axis.
    // The translation happens just before the  projection, so we do not need to
    // transform it in any way, we just update the values.
    mCamFrame[3][0] += dx * sensitivity;
    mCamFrame[3][1] -= dy * sensitivity;
    break;
  }

  default:
    assert("Unhandled mouse movement" && false);
    return;
  }
}

void Application::mouseScrollCallback(double xoffset, double yoffset) {
  (void)xoffset;
  const float sensitivity = 0.1f;
  // Same considerations as pan.
  mCamFrame[3][2] += static_cast<float>(yoffset) * sensitivity;
}

void Application::setState(std::unique_ptr<AppState> newState) {
  if (!newState) {
    throw std::invalid_argument("The state cannot be null");
  }
  mPendingState = std::move(newState);
}

Scene &Application::getScene() {
  if (!mScene) {
    throw std::runtime_error("Scene not available.");
  }
  return *mScene;
}
