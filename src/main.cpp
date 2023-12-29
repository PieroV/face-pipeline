#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <string>
#include <valarray>
#include <vector>

#include <cstdint>
#include <cstdio>

#include "glad/glad.h"

#include "GLFW/glfw3.h"

#include "glm/glm.hpp"
#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/euler_angles.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h"

#include "nlohmann/json.hpp"
using json = nlohmann::json;

const char vertShader[] = R"THE_SHADER(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform mat4 pvm;
out vec3 pointColor;

void main() {
  gl_Position = pvm * vec4(aPos.xyz, 1.0);
  pointColor = aColor;
}
)THE_SHADER";

const char fragShader[] = R"THE_SHADER(
#version 330 core
uniform bool paintUniform;
uniform vec3 uniformColor;
in vec3 pointColor;
out vec4 FragColor;

void main()
{
  if (paintUniform) {
    FragColor = vec4(uniformColor, 1.0f);
  } else {
    FragColor = vec4(pointColor, 1.0f);
  }
}
)THE_SHADER";

static void glfwErrorCallback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

bool mouseCaptured = false;
double mouseDX = 0, mouseDY = 0;

void keyCallback(GLFWwindow *window, int key, int /* scancode */, int action,
                 int /* mods */) {
  /*if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
  }*/
  if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
    mouseCaptured = !mouseCaptured;
    if (mouseCaptured) {
      glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      int w_2 = 0, h_2 = 0;
      glfwGetWindowSize(window, &w_2, &h_2);
      w_2 /= 2;
      h_2 /= 2;
      mouseDX = 0;
      mouseDY = 0;
      glfwSetCursorPos(window, w_2, h_2);
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
  }
}

void mousePosCallback(GLFWwindow *window, double x, double y) {
  if (mouseCaptured) {
    int w_2 = 0, h_2 = 0;
    glfwGetWindowSize(window, &w_2, &h_2);
    w_2 /= 2;
    h_2 /= 2;
    mouseDX = x - w_2;
    mouseDY = y - h_2;
    glfwSetCursorPos(window, w_2, h_2);
  }
}

std::mt19937 rng;

namespace glm {
void to_json(json &j, const vec3 &v) { j = json{v.x, v.y, v.z}; }

void from_json(const json &j, vec3 &v) {
  j.at(0).get_to(v.x);
  j.at(1).get_to(v.y);
  j.at(2).get_to(v.z);
}

void to_json(json &j, const mat4 &m) {
  j = std::valarray(glm::value_ptr(m), 16);
}

void from_json(const json &j, mat4 &m) {
  std::array<float, 16> data;
  j.get_to(data);
  m = make_mat4(data.data());
}
} // namespace glm

struct PointCloud {
  std::string filename;
  glm::vec3 translationPre;
  glm::vec3 euler;
  glm::vec3 translationPost;
  glm::vec3 color;
  glm::mat4 matrix;
  bool rawMatrix;
  bool hidden;
  GLsizei numPoints = 0;

  PointCloud(const std::string &fname)
      : filename(fname), translationPre(0, 0, 0), euler(0, 0, 0),
        translationPost(0, 0, 0), matrix(1.0f), rawMatrix(false),
        hidden(false) {
    loadNumPoints();
    setRandomColor();
  }

  PointCloud(const json &j) {
    j.at("filename").get_to(filename);
    j.at("translationPre").get_to(translationPre);
    j.at("euler").get_to(euler);
    j.at("translationPost").get_to(translationPost);
    j.at("matrix").get_to(matrix);
    j.at("rawMatrix").get_to(rawMatrix);
    j.at("hidden").get_to(hidden);
    j.at("color").get_to(color);
    loadNumPoints();
  }

  glm::mat4 getMatrix() const {
    if (rawMatrix) {
      return matrix;
    }
    glm::mat4 mtx =
        glm::translate(glm::mat4(1.0f), translationPost) *
        glm::eulerAngleYXZ(glm::radians(euler.y), glm::radians(euler.x),
                           glm::radians(euler.z));
    return glm::translate(mtx, translationPre);
  }

  void load(std::vector<char> &data) const {
    std::ifstream stream(filename.c_str());
    data.insert(data.end(), std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
  }

  void loadNumPoints() {
    std::vector<char> data;
    load(data);
    numPoints = data.size() / (6 * 4);
  }

  void setRandomColor() {
    std::uniform_real_distribution<float> d(0.0, 1.0);
    color.x = d(rng);
    color.y = d(rng);
    color.z = d(rng);
  }

  json toJson() const {
    return {{"filename", filename},  {"translationPre", translationPre},
            {"euler", euler},        {"translationPost", translationPost},
            {"matrix", getMatrix()}, {"rawMatrix", rawMatrix},
            {"hidden", hidden},      {"color", color}};
  }
};

std::vector<PointCloud> clouds;

// clang-format off
static const float axes[] = {
  0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
  1.0, 0.0, 0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 0.0, 0.0, 1.0, 0.0,
  0.0, 1.0, 0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 0.0, 0.0, 1.0,
  0.0, 0.0, 1.0, 0.0, 0.0, 1.0,
};
// clang-format on

void refreshBuffer() {
  std::vector<char> buffer;
  buffer.assign((char *)axes, ((char *)axes) + sizeof(axes));
  for (const auto &cloud : clouds) {
    cloud.load(buffer);
  }
  glBufferData(GL_ARRAY_BUFFER, buffer.size(), buffer.data(), GL_STATIC_DRAW);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s data-file\n", argv[0]);
    return 1;
  }

  {
    std::random_device rd;
    rng.seed(rd());
  }

  glfwSetErrorCallback(glfwErrorCallback);
  if (!glfwInit()) {
    return 2;
  }

  const char *glslVersion = "#version 330 core";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  GLFWwindow *window = glfwCreateWindow(1280, 720, "Aligner", nullptr, nullptr);
  if (!window) {
    return 3;
  }
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    puts("Failed to initialize GLAD");
    return 4;
  }

  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, mousePosCallback);

  GLuint shader = glCreateProgram();
  GLint pvmLoc = -1, paintUniformLoc = -1, uniformColorLoc = -1;
  {
    GLint success = 0;
    const char *vs = (const char *)vertShader;
    const char *fs = (const char *)fragShader;
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vs, nullptr);
    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(vert, sizeof(infoLog), nullptr, infoLog);
      fprintf(stderr, "Cannot compile the vertex shader: %s\n", infoLog);
      return 5;
    }
    success = 0;

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fs, nullptr);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(frag, sizeof(infoLog), nullptr, infoLog);
      fprintf(stderr, "Cannot compile the fragment shader: %s\n", infoLog);
      return 6;
    }
    success = 0;

    glAttachShader(shader, vert);
    glAttachShader(shader, frag);
    glLinkProgram(shader);
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetProgramInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
      fprintf(stderr, "Cannot link the shader: %s\n", infoLog);
      return 7;
    }
    success = 0;

    glDeleteShader(vert);
    glDeleteShader(frag);

    pvmLoc = glGetUniformLocation(shader, "pvm");
    paintUniformLoc = glGetUniformLocation(shader, "paintUniform");
    uniformColorLoc = glGetUniformLocation(shader, "uniformColor");
  }

  try {
    std::ifstream i(argv[1]);
    json j;
    i >> j;
    for (const auto &p : j) {
      try {
        clouds.emplace_back(p);
      } catch (std::exception &e) {
        fprintf(stderr, "Ignoring one of the clouds: %s\n", e.what());
      }
    }
  } catch (std::exception &e) {
    fprintf(stderr, "Cannot load the JSON data: %s\n", e.what());
  }

  GLuint VBO = 0, VAO = 0;
  {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    refreshBuffer();
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.IniFilename = nullptr;
  ImGui::StyleColorsLight();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  glm::vec3 camPos(0.0f, 0.0f, 0.0f);
  float camVert = 0.0f, camHor = 0.0f;
  bool editing = false;
  size_t editIndex = 0;
  bool showDemo = false;
  bool paintUniform = false;
  std::string pcdFilename;

  double lastTime = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    if (mouseCaptured) {
      io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
      io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }
    double now = glfwGetTime();
    float dt = static_cast<float>(now - lastTime);
    lastTime = now;

    glm::vec3 lookDir;
    {
      constexpr float sensitivity = 0.005f;
      constexpr float speed = 1.0f;

      camHor = fmod(camHor - mouseDX * sensitivity, 2 * glm::pi<double>());
      camVert = glm::clamp<float>(camVert - mouseDY * sensitivity,
                                  -80.0f / 180.0f * glm::pi<float>(),
                                  80.0f / 180.0f * glm::pi<float>());
      mouseDX = 0;
      mouseDY = 0;

      const int w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
      const int a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
      const int s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
      const int d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
      const int q = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
      const int e = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
      glm::mat4 T =
          glm::rotate(glm::mat4(1.0f), camHor, glm::vec3(0.0f, 1.0f, 0.0f));
      lookDir = glm::vec3(T * glm::rotate(glm::mat4(1.0f), camVert,
                                          glm::vec3(0.0f, 0.0f, 1.0f))[0]);

      if (!io.WantCaptureKeyboard) {
        const glm::vec4 localMove =
            glm::vec4((w - s), (e - q), (d - a), 0.0) * speed * dt;
        camPos += glm::vec3(T * localMove);
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (showDemo) {
      ImGui::ShowDemoWindow(&showDemo);
    }

    {
      ImGui::Begin("Main");
      ImGui::InputFloat3("Camera Position", glm::value_ptr(camPos));
      ImGui::InputFloat("Camera Horizontal", &camHor);
      ImGui::InputFloat("Camera Vertical", &camVert);

      if (ImGui::BeginTable("clouds-table", 4)) {
        ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Delete", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Hide", ImGuiTableColumnFlags_WidthFixed);
        for (size_t i = 0; i < clouds.size(); i++) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("%zu %s", i, clouds[i].filename.c_str());
          ImGui::TableSetColumnIndex(1);
          char id[50];
          const auto &c = clouds[i].color;
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(c.x, c.y, c.z, 1.0f));
          snprintf(id, sizeof(id), "Edit##%zu", i);
          if (ImGui::Button(id)) {
            editing = true;
            editIndex = i;
          }
          ImGui::PopStyleColor(1);
          ImGui::TableSetColumnIndex(2);
          snprintf(id, sizeof(id), "Delete##%zu", i);
          if (ImGui::Button(id)) {
            if (editing && editIndex == i) {
              editing = false;
            }
            clouds.erase(clouds.begin() + i);
            i--;
            glBindVertexArray(VAO);
            refreshBuffer();
            glBindVertexArray(0);
          }
          ImGui::TableSetColumnIndex(3);
          snprintf(id, sizeof(id), "Hidden##%zu", i);
          ImGui::Checkbox(id, &clouds[i].hidden);
        }
        ImGui::EndTable();
      }

      if (ImGui::Button("Add")) {
        pcdFilename.clear();
        ImGui::OpenPopup("Add point cloud");
      }

      // Always center this window when appearing
      ImVec2 center = ImGui::GetMainViewport()->GetCenter();
      ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

      if (ImGui::BeginPopupModal("Add point cloud", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Filename", &pcdFilename);
        if (ImGui::Button("OK", ImVec2(120, 0))) {
          clouds.emplace_back(pcdFilename);
          glBindVertexArray(VAO);
          refreshBuffer();
          glBindVertexArray(0);
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      if (ImGui::Button("Save")) {
        std::ofstream o(argv[1]);
        json j;
        for (const PointCloud &pcd : clouds) {
          j.push_back(pcd.toJson());
        }
        o << std::setw(2) << j;
      }

      ImGui::Checkbox("Paint uniform", &paintUniform);
      ImGui::Checkbox("Demo window", &showDemo);
      ImGui::End();
    }

    if (editIndex >= clouds.size()) {
      editing = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(400, 120));
    if (editing && ImGui::Begin("Edit", &editing)) {
      if (!clouds[editIndex].rawMatrix) {
        ImGui::InputFloat3("Translation pre",
                           glm::value_ptr(clouds[editIndex].translationPre));
        ImGui::DragFloat3("Rotation", glm::value_ptr(clouds[editIndex].euler),
                          0.5f, -360.0f, 360.0f);
        ImGui::InputFloat3("Translation post",
                           glm::value_ptr(clouds[editIndex].translationPost));
      }
      ImGui::ColorEdit3("Color", glm::value_ptr(clouds[editIndex].color));
      ImGui::Checkbox("Raw matrix", &clouds[editIndex].rawMatrix);
      ImGui::End();
    }
    ImGui::PopStyleVar();

    // Rendering
    ImGui::Render();

    glm::mat4 proj;
    {
      int w = 0, h = 0;
      glfwGetFramebufferSize(window, &w, &h);
      glViewport(0, 0, w, h);
      proj = glm::perspective(glm::radians(45.0f), (float)w / (float)h, 0.1f,
                              100.0f);
    }
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glUseProgram(shader);
    glBindVertexArray(VAO);
    glm::mat4 view =
        glm::lookAt(camPos, camPos + lookDir, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 pv = proj * view;
    glUniformMatrix4fv(pvmLoc, 1, GL_FALSE, glm::value_ptr(pv));
    glUniform1i(paintUniformLoc, 0);
    glDrawArrays(GL_LINES, 0, 6);
    glUniform1i(paintUniformLoc, paintUniform);

    GLsizei offset = 6;
    for (const auto &pcd : clouds) {
      if (!pcd.hidden) {
        glm::mat4 pvm = pv * pcd.getMatrix();
        glUniformMatrix4fv(pvmLoc, 1, GL_FALSE, glm::value_ptr(pvm));
        glUniform3fv(uniformColorLoc, 1, glm::value_ptr(pcd.color));
        glDrawArrays(GL_POINTS, offset, pcd.numPoints);
      }
      offset += pcd.numPoints;
    }
    glBindVertexArray(0);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
