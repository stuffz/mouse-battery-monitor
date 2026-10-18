# Makefile for Mouse Battery Monitor (Windows + Linux)

CXX ?= g++
BUILD_DATE := $(shell date +"%Y-%m-%d %H:%M:%S")
GIT_HASH ?= $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")

DEBUG ?= 0
PREFIX ?= /usr/local
DESTDIR ?=

SRC_DIR = src
BUILD_BASE = build
RESOURCE_DIR = resources

ifeq ($(DEBUG), 1)
    BUILD_DIR = $(BUILD_BASE)/debug
else
    BUILD_DIR = $(BUILD_BASE)/release
endif

OBJ_DIR = $(BUILD_DIR)/obj

UNAME_S := $(shell uname -s 2>/dev/null)

ifeq ($(OS),Windows_NT)
    DETECTED_PLATFORM = windows
else ifneq (,$(findstring MINGW,$(UNAME_S)))
    DETECTED_PLATFORM = windows
else ifneq (,$(findstring MSYS,$(UNAME_S)))
    DETECTED_PLATFORM = windows
else ifneq (,$(findstring CYGWIN,$(UNAME_S)))
    DETECTED_PLATFORM = windows
else
    DETECTED_PLATFORM = $(shell echo "$(UNAME_S)" | tr 'A-Z' 'a-z')
endif

# Override to cross-compile, e.g.
#   make PLATFORM=windows CXX=x86_64-w64-mingw32-g++ WINDRES=x86_64-w64-mingw32-windres
PLATFORM ?= $(DETECTED_PLATFORM)
WINDRES ?= windres

WARNINGS = -Wall -Wextra -Wsuggest-override -Wnon-virtual-dtor -Wshadow

COMMON_CXXFLAGS = -std=c++17 -O2 $(WARNINGS) -MMD -MP -I$(SRC_DIR) \
	-DBUILD_DATE="\"$(BUILD_DATE)\"" -DGIT_HASH="\"$(GIT_HASH)\""

# ---------------------------------------------------------------- Windows ----
ifeq ($(PLATFORM),windows)

TARGET_NAME = MouseBatteryMonitor.exe
SOURCES = $(SRC_DIR)/main_win32.cpp
CXXFLAGS = $(COMMON_CXXFLAGS)
LDFLAGS = -static
LIBS = -lhid -lsetupapi -lgdi32 -lshell32 -luser32 -lgdiplus -lpthread
RESOURCE_OBJ = $(OBJ_DIR)/app.res

ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -DDEBUG
    LDFLAGS :=
else
    LDFLAGS += -mwindows
endif

# ------------------------------------------------------------------ Linux ----
else

TARGET_NAME = mouse-battery-monitor
SOURCES = $(SRC_DIR)/main_linux.cpp
RESOURCE_OBJ =

PKGS = Qt6Widgets libudev
# -isystem, not -I: keeps Qt's own header warnings out of our build output.
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS) 2>/dev/null | sed 's/-I/-isystem /g')
PKG_LIBS := $(shell pkg-config --libs $(PKGS) 2>/dev/null)

CXXFLAGS = $(COMMON_CXXFLAGS) -fPIC $(PKG_CFLAGS)
LDFLAGS =
LIBS = $(PKG_LIBS) -lpthread

# lint reads the tests' compile database, which only test-build writes.
LINT_DEPS = test-build

ifeq ($(DEBUG), 1)
    CXXFLAGS += -g -DDEBUG
endif

endif

TARGET = $(BUILD_DIR)/$(TARGET_NAME)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
DEPS = $(OBJECTS:.o=.d)

.PHONY: all build clean run help deps lint format format-check install uninstall \
	install-udev-rules uninstall-udev-rules test test-build image container-build \
	container-test container-lint container-format-check

FORMAT_SOURCES := $(shell git ls-files '*.cpp' '*.hpp')

# Recursive submake so the clean finishes before any directory is recreated.
all:
	$(MAKE) clean
	$(MAKE) build

build: $(TARGET)

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

$(TARGET): $(OBJECTS) $(RESOURCE_OBJ) | $(BUILD_DIR)
	echo Linking $@...
	$(CXX) $(OBJECTS) $(RESOURCE_OBJ) -o $@ $(LDFLAGS) $(LIBS)
	echo Copying resources and config...
	mkdir -p "$(BUILD_DIR)/resources"
	cp "$(RESOURCE_DIR)"/*.png "$(BUILD_DIR)/resources/"
	test -f "$(BUILD_DIR)/config.ini" || cp "config.ini.example" "$(BUILD_DIR)/config.ini"
	echo Build complete: $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	echo Compiling $<...
	$(CXX) $(CXXFLAGS) -c $< -o $@

ifeq ($(PLATFORM),windows)
$(RESOURCE_OBJ): $(RESOURCE_DIR)/app.rc | $(OBJ_DIR)
	echo Compiling resources...
	$(WINDRES) $(RESOURCE_DIR)/app.rc -O coff -o $(RESOURCE_OBJ)
endif

-include $(DEPS)

clean:
	echo Cleaning build files...
	rm -rf "$(OBJ_DIR)" "$(TARGET)" *.log

run: build
	echo Running $(TARGET)...
	$(TARGET)

# Report missing Linux build dependencies with their package names.
deps:
ifeq ($(PLATFORM),windows)
	echo "Windows build needs MinGW-w64 GCC and make."
else
	@pkg-config --exists Qt6Widgets || \
		echo "MISSING Qt6Widgets  (Arch: qt6-base, Debian: qt6-base-dev, Fedora: qt6-qtbase-devel)"
	@pkg-config --exists libudev || \
		echo "MISSING libudev     (Arch: systemd-libs, Debian: libudev-dev, Fedora: systemd-devel)"
	@pkg-config --exists Qt6Widgets && pkg-config --exists libudev && \
		echo "All build dependencies present." || true
endif

format:
	@command -v clang-format >/dev/null || { echo "clang-format not found"; exit 1; }
	clang-format -i $(FORMAT_SOURCES)

format-check:
	@command -v clang-format >/dev/null || { echo "clang-format not found"; exit 1; }
	clang-format --dry-run --Werror $(FORMAT_SOURCES)

# Static analysis. Checks and exclusions are configured in .clang-tidy.
#
# Headers are linted as standalone TUs: misc-include-cleaner only diagnoses the
# main file, so a header-only project gets no include checking otherwise.
# Windows-only headers are skipped; they cannot compile on Linux.
LINT_HEADERS := $(shell git ls-files 'src/*.hpp' | \
	grep -vE '(win32|app_window|context_menu)')

# The tests go through the database test-build exports instead: each one
# includes moc output that exists only in the test build tree.
LINT_TESTS := $(shell find tests -name '*.cpp' | sort)

lint: $(LINT_DEPS)
ifeq ($(PLATFORM),windows)
	echo "lint is only supported on Linux."
else
	@command -v clang-tidy >/dev/null || { echo "clang-tidy not found (Arch: clang)"; exit 1; }
	@printf '%s\n' $(SOURCES) $(LINT_HEADERS) | xargs -P $$(nproc) -I{} \
		clang-tidy --quiet --warnings-as-errors='*' {} -- \
		-xc++ -Wno-pragma-once-outside-header $(CXXFLAGS)
	@printf '%s\n' $(LINT_TESTS) | xargs -P $$(nproc) -I{} \
		clang-tidy --quiet --warnings-as-errors='*' -p tests {}
	@echo "clang-tidy clean ($(words $(SOURCES) $(LINT_HEADERS) $(LINT_TESTS)) files)"
	@if command -v cppcheck >/dev/null; then \
		cppcheck --quiet --enable=warning,performance,portability --inline-suppr \
			--suppress=missingIncludeSystem --error-exitcode=1 \
			--std=c++17 -I$(SRC_DIR) $(SOURCES); \
	else \
		echo "cppcheck not found, skipping (Arch: cppcheck)"; \
	fi
endif

# ------------------------------------------------------------------ tests ----
# CMake rather than this Makefile: QTest needs moc, and CMAKE_AUTOMOC plus
# CTest is the whole of that rule. tests/ is its own CMake project so nothing
# suggests the app builds any other way than the rules above.
CMAKE ?= cmake
CTEST ?= ctest
# Host and container need separate directories: a CMake cache records the
# absolute path it was configured at, and the container's is /work.
TEST_DIR ?= $(BUILD_BASE)/test
CONTAINER_TEST_DIR = $(BUILD_BASE)/test-container

# Qt6 adds this for itself, in the spelling GCC accepts. clangd and clang-tidy
# are clang and reject it, so the database they read loses it.
GCC_ONLY_FLAG = -mno-direct-extern-access

ifeq ($(PLATFORM),windows)
test test-build:
	echo "tests are only supported on Linux."
else
# The compile database lands in tests/, not the root: clangd picks the nearest
# one, so the tests get their moc include paths from it while src/ keeps
# reading compile_flags.txt.
test-build:
	$(CMAKE) -S tests -B "$(TEST_DIR)" -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	$(CMAKE) --build "$(TEST_DIR)" --parallel
	sed 's/ $(GCC_ONLY_FLAG)//g' "$(TEST_DIR)/compile_commands.json" > tests/compile_commands.json

test: test-build
	$(CTEST) --test-dir "$(TEST_DIR)" --output-on-failure
endif

# -------------------------------------------------------------- container ----
# Everything the build needs lives in the image; nothing installs on the host.
DOCKER ?= docker
IMAGE = mouse-battery-monitor-build
DOCKER_RUN = $(DOCKER) run --rm -u $(shell id -u):$(shell id -g) \
	-e TEST_DIR="$(CONTAINER_TEST_DIR)" -v "$(CURDIR)":/work -w /work $(IMAGE)

image:
	$(DOCKER) build -t $(IMAGE) .

container-build: image
	$(DOCKER_RUN) make build

# /work is this directory bind mounted, so rewriting the paths makes the
# database name the same files, generated moc output included.
container-test: image
	$(DOCKER_RUN) make test
	sed 's|/work|$(CURDIR)|g' "$(CONTAINER_TEST_DIR)/compile_commands.json" | \
		sed 's/ $(GCC_ONLY_FLAG)//g' > tests/compile_commands.json

container-lint: image
	$(DOCKER_RUN) make lint

container-format-check: image
	$(DOCKER_RUN) make format-check

# ---------------------------------------------------------------- install ----
install: build
ifeq ($(PLATFORM),windows)
	echo "install is only supported on Linux."
else
	install -Dm755 "$(TARGET)" "$(DESTDIR)$(PREFIX)/bin/$(TARGET_NAME)"
	install -d "$(DESTDIR)$(PREFIX)/share/mouse-battery-monitor/resources"
	install -m644 $(RESOURCE_DIR)/*.png "$(DESTDIR)$(PREFIX)/share/mouse-battery-monitor/resources/"
	install -Dm644 "config.ini.example" \
		"$(DESTDIR)$(PREFIX)/share/mouse-battery-monitor/config.ini.example"
	install -Dm644 "$(RESOURCE_DIR)/battery_100.png" \
		"$(DESTDIR)$(PREFIX)/share/icons/hicolor/32x32/apps/mouse-battery-monitor.png"
	install -d "$(DESTDIR)$(PREFIX)/share/applications" \
		"$(DESTDIR)$(PREFIX)/lib/systemd/user"
	sed 's|@PREFIX@|$(PREFIX)|g' "$(RESOURCE_DIR)/linux/mouse-battery-monitor.desktop" \
		> "$(DESTDIR)$(PREFIX)/share/applications/mouse-battery-monitor.desktop"
	sed 's|@PREFIX@|$(PREFIX)|g' "$(RESOURCE_DIR)/linux/mouse-battery-monitor.service" \
		> "$(DESTDIR)$(PREFIX)/lib/systemd/user/mouse-battery-monitor.service"
	chmod 644 "$(DESTDIR)$(PREFIX)/share/applications/mouse-battery-monitor.desktop" \
		"$(DESTDIR)$(PREFIX)/lib/systemd/user/mouse-battery-monitor.service"
	echo "Installed to $(DESTDIR)$(PREFIX)"
	echo "Run 'sudo make install-udev-rules' once to grant HID access."
endif

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/$(TARGET_NAME)"
	rm -rf "$(DESTDIR)$(PREFIX)/share/mouse-battery-monitor"
	rm -f "$(DESTDIR)$(PREFIX)/share/icons/hicolor/32x32/apps/mouse-battery-monitor.png"
	rm -f "$(DESTDIR)$(PREFIX)/share/applications/mouse-battery-monitor.desktop"
	rm -f "$(DESTDIR)$(PREFIX)/lib/systemd/user/mouse-battery-monitor.service"
	echo "Uninstalled from $(DESTDIR)$(PREFIX)"

# Needs root: /dev/hidraw* is root-only until a rule tags our devices.
install-udev-rules:
	install -Dm644 "$(RESOURCE_DIR)/linux/70-mouse-battery-monitor.rules" \
		"$(DESTDIR)/etc/udev/rules.d/70-mouse-battery-monitor.rules"
	udevadm control --reload-rules
	udevadm trigger --subsystem-match=hidraw --action=change
	echo "udev rules installed. Replug the device if it is still inaccessible."

uninstall-udev-rules:
	rm -f "$(DESTDIR)/etc/udev/rules.d/70-mouse-battery-monitor.rules"
	udevadm control --reload-rules
	echo "udev rules removed."

help:
	@echo Mouse Battery Monitor - Build System
	@echo
	@echo "Detected platform: $(PLATFORM)"
	@echo
	@echo Targets:
	@echo "  all                   - Clean and build (default)"
	@echo "  build                 - Build without cleaning"
	@echo "  clean                 - Remove build artifacts"
	@echo "  run                   - Build and run"
	@echo "  test                  - Configure with CMake and run the CTest suite (Linux)"
	@echo "  image                 - Build the build container image"
	@echo "  container-build       - Build inside the container"
	@echo "  container-test        - Run the CTest suite inside the container"
	@echo "  container-lint        - Run clang-tidy inside the container"
	@echo "  container-format-check- Run clang-format check inside the container"
	@echo "  deps                  - Check build dependencies (Linux)"
	@echo "  format                - Rewrite sources with clang-format"
	@echo "  format-check          - Fail if any source is not clang-format clean"
	@echo "  install               - Install under PREFIX (Linux, default /usr/local)"
	@echo "  uninstall             - Remove installed files"
	@echo "  install-udev-rules    - Install HID access rules (Linux, needs root)"
	@echo "  uninstall-udev-rules  - Remove HID access rules (Linux, needs root)"
	@echo "  help                  - Show this help"
	@echo
	@echo Options:
	@echo "  DEBUG=1               - Build with debug symbols and verbose logging"
	@echo "  PREFIX=/usr           - Install prefix"
	@echo
	@echo Examples:
	@echo "  make"
	@echo "  make DEBUG=1"
	@echo "  make run"
	@echo "  sudo make install install-udev-rules PREFIX=/usr"
