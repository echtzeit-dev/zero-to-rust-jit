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

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
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

if (${LLVM_PACKAGE_VERSION} VERSION_LESS 18)
  set(LLVM_CAPI_HAS_DEBUG_REGISTRATION False)
else()
  set(LLVM_CAPI_HAS_DEBUG_REGISTRATION True)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/config.h")

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
  COMMAND ${LLVM_TOOLS_BINARY_DIR}/clang -c -g -emit-llvm -isysroot ${sdk_path} -o ${bitcode_sum_c} ${source_sum_c}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS ${source_sum_c}
  COMMENT "Precompiling ${source_sum_c}"
  VERBATIM
)

execute_process(COMMAND rustc --version -v
  ERROR_VARIABLE RUSTC_ERROR_OUTPUT
  OUTPUT_VARIABLE RUSTC_VERSION_VERBOSE
  OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCH "LLVM version: [0-9\\.]+" RUSTC_LLVM_VERSION_LINE "${RUSTC_VERSION_VERBOSE}")
if("${RUSTC_LLVM_VERSION_LINE}" STREQUAL "")
  message(FATAL_ERROR "Failed to query rustc version. "
                      "Please check `rustup which rustc` and `rustc --version -v` and try again.")
endif()

string(REPLACE " " ";" RUSTC_LLVM_VERSION_SPLIT ${RUSTC_LLVM_VERSION_LINE})
list(GET RUSTC_LLVM_VERSION_SPLIT -1 RUSTC_LLVM_VERSION)
message(STATUS "Rust LLVM version: ${RUSTC_LLVM_VERSION}")
if(NOT ${RUSTC_LLVM_VERSION} MATCHES "${LLVM_VERSION_MAJOR}\\.[0-9]+\\..*")
  message(WARNING "Provided LLVM doesn't match the version of your installed rustc!")
endif()

set(source_sum_rs ${CMAKE_CURRENT_SOURCE_DIR}/sum.rs)
set(bitcode_sum_rs ${CMAKE_CURRENT_BINARY_DIR}/sum_rs.bc)
add_custom_command(OUTPUT ${bitcode_sum_rs}
  COMMAND rustc --crate-type=lib -O --emit=llvm-bc -g -o ${bitcode_sum_rs} ${source_sum_rs}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  DEPENDS ${source_sum_rs}
  COMMENT "Precompiling ${source_sum_rs}"
  VERBATIM
)

add_custom_target(precompile_sum_c DEPENDS ${bitcode_sum_c})
add_custom_target(precompile_sum_rs DEPENDS ${bitcode_sum_rs})
add_dependencies(zero-to-rust-jit precompile_sum_c precompile_sum_rs)
