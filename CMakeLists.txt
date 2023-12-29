cmake_minimum_required(VERSION 3.5)
project(FacePipeline)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES src/*.cpp)

LIST(APPEND DEPS_INCLUDES thirdparty/glad/include)
file(GLOB glad_src thirdparty/glad/src/*.c)
LIST(APPEND SOURCES ${glad_src})

LIST(APPEND DEPS_INCLUDES thirdparty/json/single_include)

set(IMGUI_DIR thirdparty/imgui)
LIST(APPEND DEPS_INCLUDES ${IMGUI_DIR} ${IMGUI_DIR}/backends ${IMGUI_DIR}/misc/cpp)
LIST(APPEND SOURCES ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp)
LIST(APPEND SOURCES ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
LIST(APPEND SOURCES ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp)

find_package(glfw3 3.3 REQUIRED)

add_executable(align ${SOURCES})
target_include_directories(align SYSTEM PUBLIC ${DEPS_INCLUDES})
target_link_libraries(align glfw)
target_compile_options(align PRIVATE -Wall -Wextra -Wpedantic -Werror)
