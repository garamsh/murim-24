.PHONY: lint format test build ci

BUILD_DIR := build
EXTENSION_BUILD_DIR := $(BUILD_DIR)/extension
TOOLS := $(CURDIR)/.venv/bin
GODOT ?= godot
CXX_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp' 'core/*.hpp' 'extension/*.cpp' 'extension/*.hpp'
CXX_SOURCE_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp'
EXTENSION_SOURCE_SCOPE := git ls-files --cached --others --exclude-standard -- 'extension/*.cpp'
GDEXTENSION_SCOPE := git ls-files --cached --others --exclude-standard -- '*.gdextension'
GDSCRIPT_TEST_SCOPE := git ls-files --cached --others --exclude-standard -- '*_test.gd'

# CMAKE_OSX_SYSROOT puts the macOS SDK into compile_commands.json so clang-tidy finds the standard headers; other hosts ignore it.
# godot-cpp replaces CMAKE_OSX_SYSROOT with $$SDKROOT, so the extension build passes the SDK there instead; xcrun is absent off macOS, leaving it empty.
# build/.gdignore keeps Godot from importing the build trees, which hold godot-cpp's own test project.
# .godot/extension_list.cfg names every committed .gdextension, so Godot loads them at startup: a headless
# Godot 4.7.2 session that discovers an extension itself crashes on exit (godotengine/godot#111645).
build:
	$(TOOLS)/cmake -S core -B $(BUILD_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DCMAKE_OSX_SYSROOT=macosx
	$(TOOLS)/cmake --build $(BUILD_DIR)
	SDKROOT=$$(xcrun --show-sdk-path 2>/dev/null) $(TOOLS)/cmake -S extension -B $(EXTENSION_BUILD_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DPython3_EXECUTABLE=$(TOOLS)/python3
	$(TOOLS)/cmake --build $(EXTENSION_BUILD_DIR)
	touch $(BUILD_DIR)/.gdignore
	mkdir -p .godot && $(GDEXTENSION_SCOPE) | sed 's|^|res://|' > .godot/extension_list.cfg
	@echo "built every target in core/CMakeLists.txt into $(BUILD_DIR)/, and every default target in extension/CMakeLists.txt into $(EXTENSION_BUILD_DIR)/; registered $$($(GDEXTENSION_SCOPE) | wc -l | tr -d ' ') GDExtension, selected by: $(GDEXTENSION_SCOPE)"

lint: build
	./check-contents-list.sh
	@files=$$($(CXX_SOURCE_SCOPE)); \
	test -n "$$files" || { echo "no C++ source was read" >&2; exit 1; }; \
	$(TOOLS)/clang-tidy --quiet -p $(BUILD_DIR) $$files && \
	echo "clang-tidy checked $$(echo $$files | wc -w | tr -d ' ') C++ sources and the core/ headers they include, selected by: $(CXX_SOURCE_SCOPE)"
	@files=$$($(EXTENSION_SOURCE_SCOPE)); \
	test -n "$$files" || { echo "no extension/ C++ source was read" >&2; exit 1; }; \
	$(TOOLS)/clang-tidy --quiet --config-file=core/.clang-tidy --header-filter='/extension/[^/]+\.hpp$$' -p $(EXTENSION_BUILD_DIR) $$files && \
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
