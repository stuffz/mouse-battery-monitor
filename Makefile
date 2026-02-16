# Makefile for Mouse Battery Monitor

CXX = g++
BUILD_DATE := $(shell date +"%Y-%m-%d %H:%M:%S")
GIT_HASH ?= $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Isrc -v -DBUILD_DATE="\"$(BUILD_DATE)\"" -DGIT_HASH="\"$(GIT_HASH)\""
LDFLAGS = -static -v
LIBS = -lhid -lsetupapi -lgdi32 -lshell32 -luser32 -lgdiplus -lpthread

SRC_DIR = src
BUILD_BASE = build
ifeq ($(DEBUG), 1)
    BUILD_DIR = $(BUILD_BASE)/debug
else
    BUILD_DIR = $(BUILD_BASE)/release
endif

OBJ_DIR = $(BUILD_DIR)/obj
RESOURCE_DIR = resources

TARGET = $(BUILD_DIR)/MouseBatteryMonitor.exe

SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
RESOURCE_OBJ = $(OBJ_DIR)/app.res

DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -DDEBUG
    LDFLAGS :=
else
    LDFLAGS += -mwindows
endif

.PHONY: all clean run help

all: clean $(BUILD_DIR) $(OBJ_DIR) $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJECTS) $(RESOURCE_OBJ)
	echo Linking $@...
	$(CXX) $(OBJECTS) $(RESOURCE_OBJ) -o $@ $(LDFLAGS) $(LIBS)
	echo Copying resources and config...
	mkdir -p "$(BUILD_DIR)/resources"
	cp "$(RESOURCE_DIR)"/*.png "$(BUILD_DIR)/resources/"
	cp "config.ini.example" "$(BUILD_DIR)/config.ini"
	echo Build complete: $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	echo Compiling $<...
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(RESOURCE_OBJ): $(RESOURCE_DIR)/app.rc
	echo Compiling resources...
	windres $(RESOURCE_DIR)/app.rc -O coff -o $(RESOURCE_OBJ)

clean:
	echo Cleaning build files...
	rm -rf "$(OBJ_DIR)" "$(TARGET)" *.log

run: all
	echo Running $(TARGET)...
	$(TARGET)

help:
	@echo Mouse Battery Monitor - Build System
	@echo
	@echo Targets:
	@echo "  all        - Clean and build the application (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  run        - Clean, build, and run the application"
	@echo "  help       - Show this help"
	@echo
	@echo Options:
	@echo "  DEBUG=1    - Build with debug symbols and console window"
	@echo
	@echo Examples:
	@echo "  make"
	@echo "  make DEBUG=1"
	@echo "  make run"
	@echo "  make clean"
	@echo "  make help"
