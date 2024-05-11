cmake_minimum_required(VERSION 3.12)
project(FacePipeline)

set(CMAKE_CXX_STANDARD 17)

find_package(Open3D REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(glfw3 REQUIRED)

add_subdirectory(thirdparty)
add_subdirectory(align)
