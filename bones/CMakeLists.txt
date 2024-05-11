file(GLOB BONES_SOURCE src/*.cpp)
add_executable(bones ${BONES_SOURCE})
target_include_directories(bones PUBLIC include)
target_link_libraries(
  bones
  base
  Eigen3::Eigen
  glad
  glfw
  glm
  imgui
  ImGuizmo
  Open3D)
target_compile_options(bones PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(bones PRIVATE GLM_ENABLE_EXPERIMENTAL)
