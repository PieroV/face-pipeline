cmake_minimum_required(VERSION 3.12)
project(FacePipeline)

set(CMAKE_CXX_STANDARD 17)

find_package(Open3D REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(thirdparty)

file(GLOB SOURCES src/*.cpp)

add_executable(align ${SOURCES})
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
