#ifndef ZERO_TO_RUST_JIT_H
#define ZERO_TO_RUST_JIT_H

#include "llvm-c/BitReader.h"
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/IRReader.h"
#include "llvm-c/LLJIT.h"
#include "llvm-c/Support.h"
#include "llvm-c/Target.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdnoreturn.h>

const char *init(int argc, const char *argv[], LLVMOrcLLJITRef *Jit,
                 LLVMOrcThreadSafeContextRef *Ctx);
noreturn void shutdown(int ExitCode);

LLVMModuleRef buildModule(LLVMOrcThreadSafeContextRef Ctx);
LLVMModuleRef parseModule(const char *IRCode, LLVMOrcThreadSafeContextRef Ctx);
LLVMModuleRef loadModule(const char *FileName, LLVMOrcThreadSafeContextRef Ctx);

void addModule(LLVMOrcLLJITRef Jit, LLVMModuleRef Mod);
void loop(int (*Sum)(int, int));

int handleError(LLVMErrorRef Err);

#endif // ZERO_TO_RUST_JIT_H
