file(GLOB ALIGN_SOURCES src/*.cpp)
add_executable(align ${ALIGN_SOURCES})
target_include_directories(align SYSTEM PUBLIC include)
target_link_libraries(
  align
  Eigen3::Eigen
  glfw
  glad
  glm
  imgui
  Open3D
  natsort
  nlohmann_json)
target_compile_options(align PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_compile_definitions(align PRIVATE GLM_ENABLE_EXPERIMENTAL)
