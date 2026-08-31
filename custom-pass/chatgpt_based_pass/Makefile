LLVM_CONFIG ?= llvm-config
# GNU Make supplies a portable system C++ compiler as its built-in CXX default.
# A specific compiler can still be selected with `make CXX=clang++-14`.
CXX ?= c++

BUILD_DIR := build
PLUGIN := $(BUILD_DIR)/SimplePointsTo.so
SOURCE := SimplePointsTo.cpp

LLVM_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags)
LLVM_LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags --system-libs --libs core passes support)

.PHONY: all clean test

all: $(PLUGIN)

$(PLUGIN): $(SOURCE) | $(BUILD_DIR)
	$(CXX) -fPIC -shared $(SOURCE) -o $@ $(LLVM_CXXFLAGS) -std=c++17 $(LLVM_LDFLAGS)

$(BUILD_DIR):
	mkdir -p $@

test: $(PLUGIN)
	./tests/run_tests.sh

clean:
	rm -rf $(BUILD_DIR)
