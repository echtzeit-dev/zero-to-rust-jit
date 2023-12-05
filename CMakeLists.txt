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
  generator.c
  module-build.c
  module-parse.c
  module-load.c
  utility.c
)

target_link_libraries(zero-to-rust-jit PRIVATE LLVM)
set_target_properties(zero-to-rust-jit PROPERTIES ENABLE_EXPORTS On)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  execute_process(COMMAND xcrun --show-sdk-path
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE sdk_path
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(exit_code EQUAL "0")
    message(STATUS "Platform SDK: ${sdk_path}")
  else()
    message(FATAL_ERROR "Command failed: xcrun --show-sdk-path")
  endif()
else()
  set(sdk_path "/")
endif()

set(source_sum_c ${CMAKE_CURRENT_SOURCE_DIR}/sum.c)
set(bitcode_sum_c ${CMAKE_CURRENT_BINARY_DIR}/sum_c.bc)
add_custom_command(OUTPUT ${bitcode_sum_c}
  COMMAND ${LLVM_TOOLS_BINARY_DIR}/clang -c -emit-llvm -isysroot ${sdk_path} -o ${bitcode_sum_c} ${source_sum_c}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS ${source_sum_c}
  COMMENT "Precompiling ${source_sum_c}"
  VERBATIM
)

add_custom_target(precompile_sum_c DEPENDS ${bitcode_sum_c})
add_dependencies(zero-to-rust-jit precompile_sum_c)
