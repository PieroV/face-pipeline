add_library(glad glad/src/glad.c)
target_include_directories(glad PUBLIC glad/include)

add_subdirectory(glm)

list(
  APPEND
  IMGUI_SOURCE
  imgui/imgui.cpp
  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp)
list(APPEND IMGUI_SOURCE imgui/backends/imgui_impl_glfw.cpp
     imgui/backends/imgui_impl_opengl3.cpp)
list(APPEND IMGUI_SOURCE imgui/misc/cpp/imgui_stdlib.cpp)
add_library(imgui OBJECT ${IMGUI_SOURCE})
target_include_directories(imgui PUBLIC imgui imgui/backends imgui/misc/cpp)
target_link_libraries(imgui glfw)

list(APPEND IMGUIZMO_SOURCE ImGuizmo/ImGuizmo.cpp)
add_library(ImGuizmo OBJECT ${IMGUIZMO_SOURCE})
target_include_directories(ImGuizmo PUBLIC ImGuizmo)
target_link_libraries(ImGuizmo imgui)

add_subdirectory(json)

add_subdirectory(libigl)

add_library(natsort natsort/strnatcmp.c)
target_include_directories(natsort PUBLIC natsort)
