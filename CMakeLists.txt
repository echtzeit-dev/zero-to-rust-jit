cmake_minimum_required(VERSION 3.15)
project(zero-to-rust-jit)

find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)

message(STATUS "Found LLVM")

message("LLVM STATUS:
  Mode        ${LLVM_BUILD_TYPE}
  Version     ${LLVM_PACKAGE_VERSION}
  Definitions ${LLVM_DEFINITIONS}
  Includes    ${LLVM_INCLUDE_DIRS}
  Libraries   ${LLVM_LIBRARY_DIRS}
  Tools       ${LLVM_TOOLS_BINARY_DIR}
  Targets     ${LLVM_TARGETS_TO_BUILD}"
)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(zero-to-rust-jit
  zero-to-rust-jit.c
  module-build.c
  utility.c
)

target_link_libraries(zero-to-rust-jit PRIVATE LLVM)
