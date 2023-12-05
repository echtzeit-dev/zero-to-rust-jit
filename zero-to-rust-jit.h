#ifndef ZERO_TO_RUST_JIT_H
#define ZERO_TO_RUST_JIT_H

#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/LLJIT.h"
#include "llvm-c/Support.h"
#include "llvm-c/Target.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdnoreturn.h>

void init(int argc, const char *argv[], LLVMOrcLLJITRef *Jit,
          LLVMOrcThreadSafeContextRef *Ctx);
noreturn void shutdown(int ExitCode);

LLVMModuleRef buildModule(LLVMOrcThreadSafeContextRef Ctx);

void addModule(LLVMOrcLLJITRef Jit, LLVMModuleRef Mod);
void loop(int (*Sum)(int, int));

int handleError(LLVMErrorRef Err);

#endif // ZERO_TO_RUST_JIT_H
