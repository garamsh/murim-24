.PHONY: lint format test build ci bench sweep android export

BUILD_DIR := build
EXTENSION_BUILD_DIR := $(BUILD_DIR)/extension
BENCH_DIR := $(BUILD_DIR)/bench
LINT_DIR := $(BUILD_DIR)/lint
ANDROID_BUILD_DIR := $(BUILD_DIR)/android
ANDROID_HOME ?= $(HOME)/Library/Android/sdk
ANDROID_NDK := $(ANDROID_HOME)/ndk/28.1.13356709
MACOS_BUILD_DIR := $(BUILD_DIR)/macos
WINDOWS_BUILD_DIR := $(BUILD_DIR)/windows
MINGW_DIR := $(BUILD_DIR)/mingw
EXPORT_DIR := $(BUILD_DIR)/export
TOOLS := $(CURDIR)/.venv/bin
GODOT ?= godot
CXX_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp' 'core/*.hpp' 'packs_json/*.cpp' 'packs_json/*.hpp' 'extension/*.cpp' 'extension/*.hpp'
CXX_SOURCE_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp' 'packs_json/*.cpp'
EXTENSION_SOURCE_SCOPE := git ls-files --cached --others --exclude-standard -- 'extension/*.cpp'
GDEXTENSION_SCOPE := git ls-files --cached --others --exclude-standard -- '*.gdextension'
GDSCRIPT_TEST_SCOPE := git ls-files --cached --others --exclude-standard -- '*_test.gd'
IMPORT_SCOPE := git ls-files --cached --others --exclude-standard -- '*.import'

# CMAKE_OSX_SYSROOT puts the macOS SDK into compile_commands.json so clang-tidy finds the standard headers; other hosts ignore it.
# godot-cpp replaces CMAKE_OSX_SYSROOT with $$SDKROOT, so the extension build passes the SDK there instead; xcrun is absent off macOS, leaving it empty.
# build/.gdignore keeps Godot from importing every build tree, all of which sit under build/ and include godot-cpp's own test project.
# .godot/extension_list.cfg names every committed .gdextension, so Godot loads them at startup: a headless
# Godot 4.7.2 session that discovers an extension itself crashes on exit (godotengine/godot#111645).
# godot --import imports the textures, fonts and translations the scenes load into .godot/, which
# a fresh checkout and CI lack; the .import file beside each source sets how it is imported.
build:
	$(TOOLS)/cmake -S core -B $(BUILD_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DCMAKE_OSX_SYSROOT=macosx
	$(TOOLS)/cmake --build $(BUILD_DIR)
	SDKROOT=$$(xcrun --show-sdk-path 2>/dev/null) $(TOOLS)/cmake -S extension -B $(EXTENSION_BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DPython3_EXECUTABLE=$(TOOLS)/python3
	$(TOOLS)/cmake --build $(EXTENSION_BUILD_DIR)
	touch $(BUILD_DIR)/.gdignore
	mkdir -p .godot && $(GDEXTENSION_SCOPE) | sed 's|^|res://|' > .godot/extension_list.cfg
	$(GODOT) --headless --path . --import > $(BUILD_DIR)/import.log 2>&1 || { cat $(BUILD_DIR)/import.log; exit 1; }
	@echo "imported $$($(IMPORT_SCOPE) | wc -l | tr -d ' ') resources into .godot/, selected by: $(IMPORT_SCOPE)"
	@echo "built every target in core/CMakeLists.txt into $(BUILD_DIR)/, and every default target in extension/CMakeLists.txt into $(EXTENSION_BUILD_DIR)/; registered $$($(GDEXTENSION_SCOPE) | wc -l | tr -d ' ') GDExtension, selected by: $(GDEXTENSION_SCOPE)"

# Lint parses on every host as on an Ubuntu 24.04 x86_64 host, against the pinned headers lint-sysroot.sh unpacks (ADR 0016):
# clang-tidy's findings depend on the standard library it parses. The configures only write compile_commands.json;
# clang++ is the platform's, and CMake asks it to compile and never to link, so it needs no target linker or libraries.
# CMake cannot detect the pointer size without linking, so x86_64's is given; godot-cpp needs it.
LINT_TARGET := -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_COMPILER_TARGET=x86_64-linux-gnu -DCMAKE_SYSROOT=$(CURDIR)/$(LINT_DIR)/sysroot \
	-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_SIZEOF_VOID_P=8
lint: build
	./check-contents-list.sh
	./lint-sysroot.sh $(LINT_DIR)/sysroot
	$(TOOLS)/cmake -S core -B $(LINT_DIR)/core -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja $(LINT_TARGET)
	$(TOOLS)/cmake -S extension -B $(LINT_DIR)/extension -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DPython3_EXECUTABLE=$(TOOLS)/python3 $(LINT_TARGET)
	$(TOOLS)/ninja -C $(LINT_DIR)/extension _deps/godot-cpp-build/generate_bindings
	@files=$$($(CXX_SOURCE_SCOPE)); \
	test -n "$$files" || { echo "no C++ source was read" >&2; exit 1; }; \
	$(TOOLS)/clang-tidy --quiet --header-filter='/(core|packs_json)/[^/]+\.hpp$$' -p $(LINT_DIR)/core $$files && \
	echo "clang-tidy checked $$(echo $$files | wc -w | tr -d ' ') C++ sources and the core/ and packs_json/ headers they include, selected by: $(CXX_SOURCE_SCOPE)"
	@files=$$($(EXTENSION_SOURCE_SCOPE)); \
	test -n "$$files" || { echo "no extension/ C++ source was read" >&2; exit 1; }; \
	$(TOOLS)/clang-tidy --quiet --config-file=core/.clang-tidy --header-filter='/extension/[^/]+\.hpp$$' -p $(LINT_DIR)/extension $$files && \
	echo "clang-tidy checked $$(echo $$files | wc -w | tr -d ' ') extension/ C++ sources and the extension/ headers they include, with core/.clang-tidy, selected by: $(EXTENSION_SOURCE_SCOPE)"

format:
	@files=$$($(CXX_SCOPE)); \
	test -n "$$files" || { echo "no C++ file was read" >&2; exit 1; }; \
	$(TOOLS)/clang-format --style=file:core/.clang-format --dry-run --Werror $$files && \
	echo "clang-format checked $$(echo $$files | wc -w | tr -d ' ') C++ files, selected by: $(CXX_SCOPE)"

test: build
	$(TOOLS)/ctest --test-dir $(BUILD_DIR) --output-on-failure --no-tests=error
	@echo "ctest ran the tests selected by: every GoogleTest case gtest_discover_tests registers in core/CMakeLists.txt"
	$(GODOT) --headless --path . --script res://run_tests.gd -- $$($(GDSCRIPT_TEST_SCOPE) | sed 's|^|res://|')
	@echo "run_tests.gd ran the GDScript tests selected by: $(GDSCRIPT_TEST_SCOPE)"

ci: lint format test build

# Not part of ci: tick and frame times depend on the machine, so they gate nothing.
# Both measurements run and report before either result decides the exit status.
bench: build
	$(TOOLS)/cmake -S core -B $(BENCH_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_BUILD_TYPE=Release
	$(TOOLS)/cmake --build $(BENCH_DIR) --target murim_core_bench
	@$(BENCH_DIR)/murim_core_bench; tick=$$?; \
	$(GODOT) --path . --script res://world_view/frame_bench.gd; frame=$$?; \
	echo "make bench ran the tick benchmark (exit $$tick) and the frame benchmark (exit $$frame)"; \
	test $$tick -eq 0 && test $$frame -eq 0

# Not part of ci: it measures how regions grow over seeds and years, which tunes data and gates nothing.
FIRST_SEED ?= 0
LAST_SEED ?= 49
YEARS ?= 100
sweep: build
	$(TOOLS)/cmake -S core -B $(BENCH_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_BUILD_TYPE=Release
	$(TOOLS)/cmake --build $(BENCH_DIR) --target murim_core_sweep
	$(BENCH_DIR)/murim_core_sweep $(FIRST_SEED) $(LAST_SEED) $(YEARS)

# Not part of ci: CI installs no Android SDK. The NDK's toolchain file selects the target; extension/CMakeLists.txt is the same for every platform.
# murim.apk opens the game on the title screen; murim_bench.apk runs frame_bench.gd at launch through the Android bench preset's
# command_line/extra_args, and murim_core_bench is pushed to the device separately.
android: build
	$(TOOLS)/cmake -S extension -B $(ANDROID_BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DPython3_EXECUTABLE=$(TOOLS)/python3 -DCMAKE_TOOLCHAIN_FILE=$(ANDROID_NDK)/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24
	$(TOOLS)/cmake --build $(ANDROID_BUILD_DIR) --target murim_extension murim_core_bench
	$(GODOT) --headless --path . --export-debug Android $(ANDROID_BUILD_DIR)/murim.apk
	$(GODOT) --headless --path . --export-debug "Android bench" $(ANDROID_BUILD_DIR)/murim_bench.apk
	@echo "built $(ANDROID_BUILD_DIR)/libmurim.android.template_debug.arm64.so and $(ANDROID_BUILD_DIR)/core/murim_core_bench for Android arm64, and exported $(ANDROID_BUILD_DIR)/murim.apk and murim_bench.apk with the Android and Android bench presets in export_presets.cfg"

# Not part of ci: it builds the macOS universal library with the host's SDK, so it runs on a Mac, and fetches
# the Windows cross toolchain mingw-toolchain.sh pins. Both libraries are release builds, which a release export loads.
EXTENSION_RELEASE := -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DPython3_EXECUTABLE=$(TOOLS)/python3 -DGODOTCPP_TARGET=template_release
export: build
	SDKROOT=$$(xcrun --show-sdk-path) $(TOOLS)/cmake -S extension -B $(MACOS_BUILD_DIR) $(EXTENSION_RELEASE) -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64'
	$(TOOLS)/cmake --build $(MACOS_BUILD_DIR) --target murim_extension
	./mingw-toolchain.sh $(MINGW_DIR)
	$(TOOLS)/cmake -S extension -B $(WINDOWS_BUILD_DIR) $(EXTENSION_RELEASE) -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
		-DCMAKE_CXX_COMPILER=$(CURDIR)/$(MINGW_DIR)/bin/x86_64-w64-mingw32-clang++
	$(TOOLS)/cmake --build $(WINDOWS_BUILD_DIR) --target murim_extension
	rm -rf $(EXPORT_DIR) && mkdir -p $(EXPORT_DIR)/macos $(EXPORT_DIR)/windows
	$(GODOT) --headless --path . --export-release macOS $(EXPORT_DIR)/macos/Murim24.app
	$(GODOT) --headless --path . --export-release Windows $(EXPORT_DIR)/windows/Murim24.exe
	@echo "built $(MACOS_BUILD_DIR)/libmurim.macos.template_release.universal.dylib and $(WINDOWS_BUILD_DIR)/libmurim.windows.template_release.x86_64.dll, and exported $(EXPORT_DIR)/macos/Murim24.app and $(EXPORT_DIR)/windows/Murim24.exe with the macOS and Windows presets in export_presets.cfg"
