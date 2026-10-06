.PHONY: lint format test build ci

BUILD_DIR := build
TOOLS := $(CURDIR)/.venv/bin
CXX_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp' 'core/*.hpp'
CXX_SOURCE_SCOPE := git ls-files --cached --others --exclude-standard -- 'core/*.cpp'

# CMAKE_OSX_SYSROOT puts the macOS SDK into compile_commands.json so clang-tidy finds the standard headers; other hosts ignore it.
build:
	$(TOOLS)/cmake -S core -B $(BUILD_DIR) -G Ninja -DCMAKE_MAKE_PROGRAM=$(TOOLS)/ninja -DCMAKE_OSX_SYSROOT=macosx
	$(TOOLS)/cmake --build $(BUILD_DIR)
	@echo "built every target in core/CMakeLists.txt into $(BUILD_DIR)/"

lint: build
	./check-contents-list.sh
	@files=$$($(CXX_SOURCE_SCOPE)); \
	test -n "$$files" || { echo "no C++ source was read" >&2; exit 1; }; \
	$(TOOLS)/clang-tidy --quiet -p $(BUILD_DIR) $$files && \
	echo "clang-tidy checked $$(echo $$files | wc -w | tr -d ' ') C++ sources and the core/ headers they include, selected by: $(CXX_SOURCE_SCOPE)"

format:
	@files=$$($(CXX_SCOPE)); \
	test -n "$$files" || { echo "no C++ file was read" >&2; exit 1; }; \
	$(TOOLS)/clang-format --dry-run --Werror $$files && \
	echo "clang-format checked $$(echo $$files | wc -w | tr -d ' ') C++ files, selected by: $(CXX_SCOPE)"

test: build
	$(TOOLS)/ctest --test-dir $(BUILD_DIR) --output-on-failure --no-tests=error
	@echo "ctest ran the tests selected by: every GoogleTest case gtest_discover_tests registers in core/CMakeLists.txt"

ci: lint format test build
