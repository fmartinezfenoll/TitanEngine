CXX := g++
CC  := gcc

RENDERER ?= opengl
UPRENDERER := $(shell echo $(RENDERER) | tr a-z A-Z)

CONFIG ?= debug

# ==============================
# CONFIGURATION
# ==============================

ifeq ($(CONFIG),release)
    BUILD_DIR := build/release
    OPTFLAGS := -O3 -DNDEBUG
else
    BUILD_DIR := build/debug
    OPTFLAGS := -O0 -g
endif

BIN := $(BUILD_DIR)/app

# ==============================
# SOURCES
# ==============================

ENGINE_SRC := $(wildcard src/*.cpp src/*/*.cpp)
ENGINE_OBJ := $(ENGINE_SRC:src/%.cpp=$(BUILD_DIR)/%.o)

GLAD_SRC := thirdparty/glad/glad.c
IMGUI_SRC := $(wildcard thirdparty/imgui/*.cpp) \
             thirdparty/imgui/backends/imgui_impl_glfw.cpp \
             thirdparty/imgui/backends/imgui_impl_opengl3.cpp

GLAD_OBJ := $(BUILD_DIR)/thirdparty/glad.o
IMGUI_OBJ := $(IMGUI_SRC:thirdparty/%.cpp=$(BUILD_DIR)/thirdparty/%.o)

THIRDPARTY_OBJ := $(GLAD_OBJ) $(IMGUI_OBJ)
THIRDPARTY_LIB := $(BUILD_DIR)/libthirdparty.a

# ==============================
# FLAGS
# ==============================

INCLUDE := -Iinclude \
           -Iexternal \
           -Ithirdparty \
           -Ithirdparty/imgui \
           -Ithirdparty/imgui/backends \
           -Ithirdparty/glfw/include \
           -Ithirdparty/glm


CXXFLAGS := -std=c++20 -Wall -Wextra -MMD -MP \
            -DIMGUI_IMPL_OPENGL_LOADER_GLAD \
            -DRENDERER_$(UPRENDERER) \
            $(OPTFLAGS) \
            $(INCLUDE)

EXTFLAGS := -std=c++20 -w $(OPTFLAGS) $(INCLUDE)

LDFLAGS :=
LIBPATH :=
LIBS :=

UNAME_S := $(shell uname -s)

# ==============================
# LINUX
# ==============================

ifeq ($(UNAME_S), Linux)
    LIBS := -lglfw -lGL -ldl -lX11 -lpthread
    RUN := ./$(BIN)

    ifeq ($(CONFIG),release)
        LDFLAGS += -static-libstdc++ -static-libgcc
    endif
endif

# ==============================
# WINDOWS (MinGW)
# ==============================

ifeq ($(OS), Windows_NT)
    LIBPATH := -Lthirdparty/glfw/lib
    LIBS := -lglfw3 -lopengl32 -lgdi32 -luser32 -lkernel32 -lshell32 -lwinmm
    RUN := $(BIN).exe

    ifeq ($(CONFIG),release)
        LDFLAGS += -static -static-libgcc -static-libstdc++
    endif
endif


# ==============================
# TARGETS
# ==============================

.PHONY: all clean run debug release

all: $(BIN)

debug:
	$(MAKE) CONFIG=debug

release:
	$(MAKE) CONFIG=release

# ==============================
# LINK
# ==============================

$(BIN): $(ENGINE_OBJ) $(THIRDPARTY_LIB)
	@mkdir -p $(BUILD_DIR)
	$(CXX) $^ -o $@ $(LDFLAGS) $(LIBS)

# ==============================
# ENGINE OBJECTS
# ==============================

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

# ==============================
# GLAD (C)
# ==============================

$(BUILD_DIR)/thirdparty/glad.o: $(GLAD_SRC)
	@mkdir -p $(BUILD_DIR)/thirdparty
	$(CC) -c $< -o $@ $(EXTFLAGS)

# ==============================
# IMGUI
# ==============================

$(BUILD_DIR)/thirdparty/%.o: thirdparty/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(EXTFLAGS)

# ==============================
# STATIC LIB
# ==============================

$(THIRDPARTY_LIB): $(THIRDPARTY_OBJ)
	ar rcs $@ $^

# ==============================
# RUN
# ==============================

run: all
	$(RUN)

# ==============================
# CLEAN
# ==============================

clean:
	rm -rf build

-include $(ENGINE_OBJ:.o=.d)
