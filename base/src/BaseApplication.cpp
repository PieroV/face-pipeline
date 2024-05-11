/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "BaseApplication.h"

#include <stdexcept>

#include "glm/gtx/euler_angles.hpp"

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "utilities.h"

static BaseApplication *appFromWindow(GLFWwindow *window) {
  return static_cast<BaseApplication *>(glfwGetWindowUserPointer(window));
}

BaseApplication::BaseApplication(const char *windowTitle) {
  glfwSetErrorCallback([](int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
  });

  try {
    initGlfw(windowTitle);
    initImgui();
  } catch (std::exception &err) {
    // GLFW is a C library and Imgui does not use exceptions.
    // So, we are the ones creating exceptions, and we are using only standard
    // ones, and we can avoid using an exception_ptr.
    terminateBase();
    throw err;
  }
  initRng();
}

BaseApplication::~BaseApplication() { terminateBase(); }

void BaseApplication::initGlfw(const char *windowTitle) {
  if (!glfwInit()) {
    throw std::runtime_error("Failed to initialize GLFW.");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  mWindow = glfwCreateWindow(1280, 720, windowTitle, nullptr, nullptr);
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
    if (BaseApplication *app = appFromWindow(window)) {
      app->keyCallback(key, scancode, action, mods);
    }
  });
  glfwSetMouseButtonCallback(
      mWindow, [](GLFWwindow *window, int button, int action, int mods) {
        if (BaseApplication *app = appFromWindow(window)) {
          app->mouseClickCallback(button, action, mods);
        }
      });
  glfwSetCursorPosCallback(mWindow, [](GLFWwindow *window, double x, double y) {
    if (BaseApplication *app = appFromWindow(window)) {
      app->mousePosCallback(x, y);
    }
  });
  glfwSetScrollCallback(mWindow,
                        [](GLFWwindow *window, double xoffset, double yoffset) {
                          if (BaseApplication *app = appFromWindow(window)) {
                            app->mouseScrollCallback(xoffset, yoffset);
                          }
                        });
}

void BaseApplication::initImgui() {
  IMGUI_CHECKVERSION();
  if (!ImGui::CreateContext()) {
    throw std::runtime_error("Failed to create an ImGui context.");
  }
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImGui::StyleColorsLight();
  if (!ImGui_ImplGlfw_InitForOpenGL(mWindow, true)) {
    ImGui::DestroyContext();
    throw std::runtime_error(
        "Failed to initialize the ImGui GLFW implementation.");
  }
  const char *glslVersion = "#version 330 core";
  if (!ImGui_ImplOpenGL3_Init(glslVersion)) {
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    throw std::runtime_error(
        "Failed to initialize the ImGui OpenGL 3 implementation.");
  }
  mHasImgui = true;
}

void BaseApplication::terminateBase() {
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

int BaseApplication::run() {
  ImGuiIO &io = ImGui::GetIO();
  while (!glfwWindowShouldClose(mWindow)) {
    beginFrame();

    glfwPollEvents();
    if (mMouseCaptured != MouseMovement::None) {
      io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
      io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }

    int w = 0, h = 0;
    glfwGetFramebufferSize(mWindow, &w, &h);
    glViewport(0, 0, w, h);

    float ratio = static_cast<float>(w) / h;
    mProjection = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);
    // Eye at center simplifies the rotation.
    mView = mCamFrame * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f),
                                    glm::vec3(0.0f, 2.0f, 0.0f),
                                    glm::vec3(0.0f, 0.0f, 1.0f));

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    createGui();
    if (mImguiDemo) {
      ImGui::ShowDemoWindow(&mImguiDemo);
    }
    ImGui::Render();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(mWindow);
  }
  return 0;
}

void BaseApplication::keyCallback(int key, int scancode, int action, int mods) {
  (void)scancode;
  if (ImGui::GetIO().WantCaptureKeyboard) {
    return;
  }
  if (key == GLFW_KEY_R && action == GLFW_PRESS) {
    mCamFrame = glm::mat4(1.0f);
  }
  if (key == GLFW_KEY_F10 && action == GLFW_PRESS) {
    mImguiDemo = !mImguiDemo;
    return;
  }
  if (key == GLFW_KEY_Q && (mods & GLFW_MOD_CONTROL)) {
    glfwSetWindowShouldClose(mWindow, GLFW_TRUE);
  }
}

void BaseApplication::mouseClickCallback(int button, int action, int mods) {
  (void)mods;
  if (ImGui::GetIO().WantCaptureMouse) {
    return;
  }
  MouseMovement mov;
  switch (button) {
  case GLFW_MOUSE_BUTTON_2:
    mov = MouseMovement::Pan;
    break;
  case GLFW_MOUSE_BUTTON_3:
    mov = MouseMovement::Rotate;
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

void BaseApplication::mousePosCallback(double x, double y) {
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
    // The translation happens just before the projection, so we do not need to
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

void BaseApplication::mouseScrollCallback(double xoffset, double yoffset) {
  (void)xoffset;
  if (ImGui::GetIO().WantCaptureMouse) {
    return;
  }
  const float sensitivity = 0.1f;
  // Same considerations as pan.
  mCamFrame[3][2] += static_cast<float>(yoffset) * sensitivity;
}
