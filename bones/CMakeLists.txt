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
  igl::core
  imgui
  ImGuizmo
  Open3D)
target_compile_options(bones PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(bones PRIVATE GLM_ENABLE_EXPERIMENTAL)

# https://github.com/libigl/libigl/pull/2254
# https://stackoverflow.com/a/13639476
set_source_files_properties(src/computeBoneWeights.cpp
                            PROPERTIES COMPILE_FLAGS -Wno-sign-compare)
