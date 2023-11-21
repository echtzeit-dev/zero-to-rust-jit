cmake_minimum_required(VERSION 3.15)
project(llvm-jit-c)

find_package(LLVM REQUIRED CONFIG NO_DEFAULT_PATH)

message(STATUS "Found LLVM ")

message("LLVM STATUS:
  Mode        ${LLVM_BUILD_TYPE}
  Version     ${LLVM_PACKAGE_VERSION}
  Definitions ${LLVM_DEFINITIONS}
  Includes    ${LLVM_INCLUDE_DIRS}
  Libraries   ${LLVM_LIBRARY_DIRS}
  Targets     ${LLVM_TARGETS_TO_BUILD}"
)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(llvm-jit-c llvm-jit.c)
llvm_map_components_to_libnames(llvm_libs OrcJIT native)
target_link_libraries(llvm-jit-c PRIVATE ${llvm_libs})