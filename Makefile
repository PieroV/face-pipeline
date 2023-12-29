INCLUDES=
SRCS=$(wildcard *.cpp)
CSRCS=

INCLUDES+=-Iglad/include
CSRCS+=glad/src/glad.c

IMGUI_DIR=imgui
INCLUDES += -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(IMGUI_DIR)/misc/cpp
SRCS += $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_demo.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp
SRCS += $(IMGUI_DIR)/backends/imgui_impl_glfw.cpp $(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp
SRCS += $(IMGUI_DIR)/misc/cpp/imgui_stdlib.cpp

INCLUDES+=-Ijson/single_include

OBJS=$(subst .cpp,.o,$(SRCS)) $(subst .c,.o,$(CSRCS))
CC=clang
CFLAGS=-Wall -Wpedantic -Wextra -g -O3 $(INCLUDES)
CXX=clang++
CXXFLAGS=$(CFLAGS)
LIBS=-lglfw
LDFLAGS=-g $(LIBS)

all: align

align: $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $@

clean:
	$(RM) $(OBJS) align
