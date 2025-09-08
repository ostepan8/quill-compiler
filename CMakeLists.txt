cmake_minimum_required(VERSION 3.20)
project(QuillCompiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set LLVM path for macOS Homebrew
if(APPLE)
    set(LLVM_DIR "/opt/homebrew/opt/llvm/lib/cmake/llvm")
    # Add LLVM tools to PATH
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/llvm")
endif()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(llvm_libs 
    support core irreader target x86codegen
    analysis passes transformutils instcombine scalaropts
)

add_executable(quill
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
    src/codegen.cpp
    src/timer.cpp
    types/type_system.cpp
    types/type_checker.cpp
    optimization/constant_folding.cpp
    optimization/dead_code_elimination.cpp
    optimization/function_inlining.cpp
    optimization/arithmetic_simplification.cpp
    optimization/type_directed_pass_impl.cpp
    optimization/optimization_manager.cpp
)

target_include_directories(quill PRIVATE include)
target_link_libraries(quill ${llvm_libs})