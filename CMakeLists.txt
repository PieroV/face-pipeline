cmake_minimum_required(VERSION 3.12)
project(FacePipeline)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES src/*.cpp)

LIST(APPEND DEPS_INCLUDES thirdparty/glad/include)
file(GLOB glad_src thirdparty/glad/src/*.c)
LIST(APPEND SOURCES ${glad_src})

LIST(APPEND DEPS_INCLUDES thirdparty/json/single_include)

LIST(APPEND DEPS_INCLUDES thirdparty/natsort)
LIST(APPEND SOURCES thirdparty/natsort/strnatcmp.c)

find_package(glfw3 3.3 REQUIRED)

find_package(glm REQUIRED)

find_package(Open3D REQUIRED)
LIST(APPEND DEPS_INCLUDES ${Open3D_INCLUDE_DIRS})

set(IMGUI_DIR thirdparty/imgui)
LIST(APPEND IMGUI_SOURCE ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp)
LIST(APPEND IMGUI_SOURCE ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
LIST(APPEND IMGUI_SOURCE ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp)
add_library(imgui OBJECT ${IMGUI_SOURCE})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMGUI_DIR}/misc/cpp)
target_link_libraries(imgui glfw3)
LIST(APPEND DEPS_INCLUDES ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMGUI_DIR}/misc/cpp)

add_executable(align ${SOURCES})
target_include_directories(align SYSTEM PUBLIC ${DEPS_INCLUDES})
target_link_libraries(align glfw glm::glm $<TARGET_OBJECTS:imgui> Open3D::Open3D)
target_compile_options(align PRIVATE -Wall -Wextra -Wpedantic)
