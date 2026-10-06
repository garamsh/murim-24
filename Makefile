.PHONY: lint format test build ci

BUILD_DIR := build
EXTENSION_BUILD_DIR := $(BUILD_DIR)/extension
TOOLS := $(CURDIR)/.venv/bin
GODOT ?= godot
CXX_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp' 'core/*.hpp' 'extension/*.cpp' 'extension/*.hpp'
CXX_SOURCE_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp'
GDSCRIPT_TEST_SCOPE := git ls-files --cached --others --exclude-standard -- '*_test.gd'

# CMAKE_OSX_SYSROOT puts the macOS SDK into compile_commands.json so clang-tidy finds the standard headers; other hosts ignore it.
# The extension build leaves it unset: godot-cpp sets its own sysroot, and clang-tidy does not read that build.
# build/.gdignore keeps Godot from importing the build trees, which hold godot-cpp's own test project.
build:
	$(TOOLS)/cmake -S core -B $(BUILD_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DCMAKE_OSX_SYSROOT=macosx
	$(TOOLS)/cmake --build $(BUILD_DIR)
	$(TOOLS)/cmake -S extension -B $(EXTENSION_BUILD_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DPython3_EXECUTABLE=$(TOOLS)/python3
	$(TOOLS)/cmake --build $(EXTENSION_BUILD_DIR)
	touch $(BUILD_DIR)/.gdignore
	@echo "built every target in core/CMakeLists.txt into $(BUILD_DIR)/, and every default target in extension/CMakeLists.txt into $(EXTENSION_BUILD_DIR)/"

lint: build
	./check-contents-list.sh
	@files=$$($(CXX_SOURCE_SCOPE)); \
	test -n "$$files" || { echo "no C++ source was read" >&2; exit 1; }; \
	$(TOOLS)/clang-tidy --quiet -p $(BUILD_DIR) $$files && \
	echo "clang-tidy checked $$(echo $$files | wc -w | tr -d ' ') C++ sources and the core/ headers they include, selected by: $(CXX_SOURCE_SCOPE)"

format:
	@files=$$($(CXX_SCOPE)); \
	test -n "$$files" || { echo "no C++ file was read" >&2; exit 1; }; \
	$(TOOLS)/clang-format --style=file:core/.clang-format --dry-run --Werror $$files && \
	echo "clang-format checked $$(echo $$files | wc -w | tr -d ' ') C++ files, selected by: $(CXX_SCOPE)"

# --import registers murim.gdextension in .godot/, which a --script run does not do by itself.
test: build
	$(TOOLS)/ctest --test-dir $(BUILD_DIR) --output-on-failure --no-tests=error
	@echo "ctest ran the tests selected by: every GoogleTest case gtest_discover_tests registers in core/CMakeLists.txt"
	$(GODOT) --headless --path . --import
	$(GODOT) --headless --path . --script res://run_tests.gd -- $$($(GDSCRIPT_TEST_SCOPE) | sed 's|^|res://|')
	@echo "run_tests.gd ran the GDScript tests selected by: $(GDSCRIPT_TEST_SCOPE)"

ci: lint format test build
