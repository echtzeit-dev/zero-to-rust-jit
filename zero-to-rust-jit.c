#include "zero-to-rust-jit.h"

const char *SumExample = "define i32 @sum(i32 %a, i32 %b) {\n"
                         "entry:\n"
                         "  %r = add nsw i32 %a, %b\n"
                         "  ret i32 %r\n"
                         "}\n";

int main(int argc, const char *argv[]) {
  // Init our bitcode JIT compiler and the LLVM context
  LLVMOrcLLJITRef Jit;
  LLVMOrcThreadSafeContextRef Ctx;
  init(argc, argv, &Jit, &Ctx);

  // Create our demo function
  LLVMModuleRef Mod = parseModule(SumExample, Ctx);
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
