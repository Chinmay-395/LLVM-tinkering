cmake_minimum_required(VERSION 3.20)
project(LLVMInspect C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find system LLVM installation
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# 2. Add LLVM definitions and header directories
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
add_definitions(${LLVM_DEFINITIONS_LIST})
include_directories(${LLVM_INCLUDE_DIRS})

# 3. Create the executable target
add_executable(inspect inspect.cpp)

# 4. Link required LLVM component libraries
# Core: Handles Module, Function, BasicBlock, Instruction data structures
# IRReader: Parses text .ll and bitcode .bc files into C++ object model
# Support: Provides raw_ostream, SMDiagnostic, memory allocation helpers
llvm_map_components_to_libnames(llvm_libs Core IRReader Support)
target_link_libraries(inspect PRIVATE ${llvm_libs})