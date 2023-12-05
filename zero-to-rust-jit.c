#include "zero-to-rust-jit.h"

int main(int argc, const char *argv[]) {
  // Init our bitcode JIT compiler and the LLVM context
  LLVMOrcLLJITRef Jit;
  LLVMOrcThreadSafeContextRef Ctx;
  init(argc, argv, &Jit, &Ctx);

  // Create our demo function
  LLVMModuleRef Mod = buildModule(Ctx);
  addModule(Jit, Mod);

  // Materialize our demo function
  LLVMErrorRef Err;
  LLVMOrcJITTargetAddress SumFnAddr;
  if ((Err = LLVMOrcLLJITLookup(Jit, &SumFnAddr, "sum")))
    shutdown(handleError(Err)); // noreturn

  // Execute our JITed code
  loop((int (*)(int, int))SumFnAddr);

  shutdown(EXIT_SUCCESS);
}
