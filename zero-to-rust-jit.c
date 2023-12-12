#include "zero-to-rust-jit.h"

void helloImpl(void) { printf("Oh hello, that's called from JITed code!\n"); }

noreturn void __rust_core_panicking_panic(const char *str) {
  fprintf(stderr, "Panic due to overflow: %s\n", str);
  abort();
}

LLVMOrcJITTargetAddress handleUndefinedSymbol(const char *MangledName) {
  if (strncmp(MangledName, "hello", 5) == 0)
    return (LLVMOrcJITTargetAddress)&helloImpl;

  while (MangledName[0] == '_')
    MangledName += 1;
  if (strncmp(MangledName, "ZN4core9panicking5panic", 23) == 0)
    return (LLVMOrcJITTargetAddress)&__rust_core_panicking_panic;

  return 0;
}

int main(int argc, const char *argv[]) {
  // Init our bitcode JIT compiler and the LLVM context
  LLVMOrcLLJITRef Jit;
  LLVMOrcThreadSafeContextRef Ctx;
  const char *FileName = init(argc, argv, &Jit, &Ctx);

  // Create our demo function
  LLVMModuleRef Mod = loadModule(FileName, Ctx);
  LLVMOrcJITDylibRef Unit = addModule(Jit, Mod);

  // Install our symbol resolver
  addGenerator(Unit, &handleUndefinedSymbol);

  // Install JITLink plugin for debug support
  addDebugSupport(Jit);

  // Materialize our demo function
  LLVMErrorRef Err;
  LLVMOrcJITTargetAddress SumFnAddr;
  if ((Err = LLVMOrcLLJITLookup(Jit, &SumFnAddr, "sum")))
    shutdown(handleError(Err)); // noreturn

  // Execute our JITed code
  loop((int (*)(int, int))SumFnAddr);

  shutdown(EXIT_SUCCESS);
}
