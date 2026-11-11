# =====================================================================
# CASEFILE - Forensic Evidence Management and Crime Investigation System
# Makefile
#
# Targets:
#   make all    compile the application
#   make run    build if needed, then start the application
#   make test   build and execute the Google Test unit tests
#   make clean  remove every generated file
#
# The build has no dependency beyond a C compiler, a C++ compiler (for the
# test target only), GNU make and Google Test.
# =====================================================================

# --- Toolchain -----------------------------------------------------
CC       := cc
CXX      := c++
AR       := ar

# --- Directories ---------------------------------------------------
SRC_DIR      := src
INC_DIR      := include
TESTS_DIR    := tests
BUILD_DIR    := build
DATA_DIR     := data
REPORT_DIR   := reports

# --- Flags ---------------------------------------------------------
# Strict warnings are mandatory for this project: the build must be clean.
CSTD      := -std=c11
WARNINGS  := -Wall -Wextra -Wpedantic
CFLAGS    := $(CSTD) $(WARNINGS) -I$(INC_DIR) -O2
CXXFLAGS  := -std=c++17 -Wall -Wextra -I$(INC_DIR) -O2
LDLIBS    :=

# --- Application ----------------------------------------------------
APP_NAME      := casefile
APP_BIN       := $(BUILD_DIR)/$(APP_NAME)
APP_SRCS      := $(wildcard $(SRC_DIR)/*.c)
APP_OBJS      := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(APP_SRCS))
APP_DEPS      := $(APP_OBJS:.o=.d)

# The module objects without main.o. The test binary links these instead of
# the application objects, because Google Test supplies its own main().
LIB_OBJS      := $(filter-out $(BUILD_DIR)/main.o,$(APP_OBJS))

# --- Tests ----------------------------------------------------------
TEST_NAME     := $(APP_NAME)_test
TEST_BIN      := $(BUILD_DIR)/$(TEST_NAME)
TEST_SRCS     := $(wildcard $(TESTS_DIR)/*.cpp)
TEST_OBJS     := $(patsubst $(TESTS_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_DEPS     := $(TEST_OBJS:.o=.d)

# --- Google Test discovery -----------------------------------------
# Google Test may be installed in several places. The lookup below tries
# each of them in turn and reports a clear message when none is found,
# rather than failing later with a confusing linker error.
#
# Override by setting GTEST_ROOT, for example:
#   make test GTEST_ROOT=/opt/homebrew/opt/googletest
GTEST_ROOT ?=
GTEST_LIB_DIR := $(if $(GTEST_ROOT),$(GTEST_ROOT)/lib,)
GTEST_INC_DIR := $(if $(GTEST_ROOT),$(GTEST_ROOT)/include,)

ifneq ($(GTEST_ROOT),)
  GTEST_FOUND := $(wildcard $(GTEST_LIB_DIR)/libgtest*.a)
  GTEST_LIBS  := $(GTEST_LIB_DIR)/libgtest.a
  GTEST_INC   := -I$(GTEST_INC_DIR)
  # Some installations ship gtest_main as a separate library; others build the
  # main function into libgtest itself. Add the separate library only when it
  # exists, so that both layouts link cleanly.
  ifneq ($(wildcard $(GTEST_LIB_DIR)/libgtest_main.a),)
    GTEST_LIBS := $(GTEST_LIB_DIR)/libgtest.a $(GTEST_LIB_DIR)/libgtest_main.a
  endif
else
  # Try the common installation prefixes, then a plain system location.
  # $(HOME) is included so that a locally built Google Test under ~/.local is
  # found without any extra configuration.
  GTEST_CANDIDATE_DIRS := /opt/homebrew/opt/googletest /usr/local/opt/googletest \
                          /opt/homebrew /usr/local /usr \
                          $(HOME)/.local $(HOME)/local $(HOME)/opt
  GTEST_FOUND :=
  GTEST_LIBS  :=
  GTEST_INC   :=
  $(foreach dir,$(GTEST_CANDIDATE_DIRS),\
    $(if $(wildcard $(dir)/lib/libgtest.a),\
      $(eval GTEST_FOUND := $(dir)/lib)\
      $(eval GTEST_LIBS := $(dir)/lib/libgtest.a)\
      $(eval GTEST_INC := -I$(dir)/include)\
      $(if $(wildcard $(dir)/lib/libgtest_main.a),\
        $(eval GTEST_LIBS := $(dir)/lib/libgtest.a $(dir)/lib/libgtest_main.a))))
endif

.PHONY: all run test clean gtest-check help

# --- Default target -------------------------------------------------
all: $(APP_BIN)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(APP_BIN): $(APP_OBJS) | $(BUILD_DIR)
	$(CC) $(APP_OBJS) -o $@ $(LDLIBS)

# --- Run ------------------------------------------------------------
run: $(APP_BIN)
	@./$(APP_BIN)

# --- Test -----------------------------------------------------------
gtest-check:
	@if [ -z "$(GTEST_LIBS)" ]; then \
		echo ""; \
		echo "  [!] Google Test was not found on this machine."; \
		echo ""; \
		echo "      The 'make test' target needs the Google Test library and headers."; \
		echo "      Install it with one of the following:"; \
		echo ""; \
		echo "        macOS (Homebrew):   brew install googletest"; \
		echo "        Linux (Debian):     sudo apt install libgtest-dev cmake"; \
		echo "        Linux (Fedora):     sudo dnf install gtest-devel"; \
		echo "        From source:        see https://github.com/google/googletest"; \
		echo ""; \
		echo "      If Google Test is installed somewhere unusual, point the build at it:"; \
		echo ""; \
		echo "        make test GTEST_ROOT=/path/to/googletest"; \
		echo ""; \
		exit 1; \
	fi

test: gtest-check $(TEST_BIN)
	@./$(TEST_BIN)

$(BUILD_DIR)/%.o: $(TESTS_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(GTEST_INC) -MMD -MP -c $< -o $@

$(TEST_BIN): $(TEST_OBJS) $(LIB_OBJS) | $(BUILD_DIR)
	$(CXX) $(TEST_OBJS) $(LIB_OBJS) -o $@ $(GTEST_LIBS) $(LDLIBS)

# --- Clean ----------------------------------------------------------
clean:
	@rm -rf $(BUILD_DIR)
	@rm -f $(DATA_DIR)/cases.txt $(DATA_DIR)/suspects.txt $(DATA_DIR)/evidence.txt \
	       $(DATA_DIR)/custody.txt $(DATA_DIR)/witnesses.txt
	@rm -rf $(REPORT_DIR)
	@echo "  Cleaned build output, generated data files and reports."

# --- Help -----------------------------------------------------------
help:
	@echo ""
	@echo "  CASEFILE - available targets"
	@echo ""
	@echo "    make all     Build the application into $(APP_BIN)"
	@echo "    make run     Build if needed, then start the application"
	@echo "    make test    Build and run the Google Test unit tests"
	@echo "    make clean   Remove every generated file"
	@echo ""
	@echo "  Google Test: $(if $(GTEST_LIBS),found at $(GTEST_LIBS),NOT FOUND)"
	@echo ""

-include $(APP_DEPS)
-include $(TEST_DEPS)
