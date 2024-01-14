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

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h"

#include "LoadState.h"

static Application *appFromWindow(GLFWwindow *window);

Application::Application() : mView(1.0f), mCamPos(0.0f, 0.0f, 0.0f) {
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
  glfwSetCursorPosCallback(mWindow, [](GLFWwindow *window, double x, double y) {
    if (Application *app = appFromWindow(window)) {
      app->mousePosCallback(x, y);
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

  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(mWindow)) {
    if (mPendingState) {
      mCurrentState.reset();
      mCurrentState = std::move(mPendingState);
    }

    assert(mCurrentState);

    glfwPollEvents();
    if (mMouseCaptured) {
      io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
      io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }
    double now = glfwGetTime();
    float dt = static_cast<float>(now - lastTime);
    lastTime = now;

    updateCamera(dt);

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
    mCurrentState->render(proj * mView);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(mWindow);
  }

  return 0;
}

void Application::updateCamera(float dt) {
  // TODO: Switch to an orbit camera
  constexpr float sensitivity = 0.005f;
  constexpr float speed = 1.0f;

  mCamHor = fmod(mCamHor - mMouseDX * sensitivity, 2 * glm::pi<double>());
  mCamVert = glm::clamp<float>(mCamVert - mMouseDY * sensitivity,
                               -80.0f / 180.0f * glm::pi<float>(),
                               80.0f / 180.0f * glm::pi<float>());
  mMouseDX = 0;
  mMouseDY = 0;
  glm::mat4 T =
      glm::rotate(glm::mat4(1.0f), mCamHor, glm::vec3(0.0f, 1.0f, 0.0f));
  glm::vec3 lookDir(T * glm::rotate(glm::mat4(1.0f), mCamVert,
                                    glm::vec3(0.0f, 0.0f, 1.0f))[0]);

  if (!ImGui::GetIO().WantCaptureKeyboard) {
    const int w = glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS;
    const int a = glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS;
    const int s = glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS;
    const int d = glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS;
    const int q = glfwGetKey(mWindow, GLFW_KEY_Q) == GLFW_PRESS;
    const int e = glfwGetKey(mWindow, GLFW_KEY_E) == GLFW_PRESS;
    const glm::vec4 localMove =
        glm::vec4((w - s), (e - q), (d - a), 0.0) * speed * dt;
    mCamPos += glm::vec3(T * localMove);
  }

  mView = glm::lookAt(mCamPos, mCamPos + lookDir, glm::vec3(0.0f, 1.0f, 0.0f));
}

void Application::keyCallback(int key, int scancode, int action, int mods) {
  (void)scancode;
  (void)mods;
  if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
    mMouseCaptured = !mMouseCaptured;
    if (mMouseCaptured) {
      glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
      glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      mousePosCallback(0, 0);
      mMouseDX = 0;
      mMouseDY = 0;
    } else {
      glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
  } else if (key == GLFW_KEY_F10 && action == GLFW_PRESS) {
    mImguiDemo = !mImguiDemo;
  }
}

void Application::mousePosCallback(double x, double y) {
  if (mMouseCaptured) {
    int w_2 = 0, h_2 = 0;
    glfwGetWindowSize(mWindow, &w_2, &h_2);
    w_2 /= 2;
    h_2 /= 2;
    mMouseDX = x - w_2;
    mMouseDY = y - h_2;
    glfwSetCursorPos(mWindow, w_2, h_2);
  }
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
