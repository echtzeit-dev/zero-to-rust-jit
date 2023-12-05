#include "zero-to-rust-jit.h"

static noreturn void DiagnosticHandler(LLVMDiagnosticInfoRef DI, void *C) {
  char *CErr = LLVMGetDiagInfoDescription(DI);
  fprintf(stderr, "Error in bitcode parser: %s\n", CErr);
  LLVMDisposeMessage(CErr);
  shutdown(EXIT_FAILURE);
}

LLVMModuleRef loadModule(const char *FileName,
                         LLVMOrcThreadSafeContextRef Ctx) {
  char *Err;
  LLVMMemoryBufferRef MemBuf;
  LLVMBool EC =
      LLVMCreateMemoryBufferWithContentsOfFile(FileName, &MemBuf, &Err);
  if (EC != 0) {
    fprintf(stderr, "Error reading file %s: %s\n", FileName, Err);
    shutdown(EC);
  }

  LLVMContextRef BareCtx = LLVMOrcThreadSafeContextGetContext(Ctx);
  LLVMContextSetDiagnosticHandler(BareCtx, DiagnosticHandler, NULL);

  LLVMModuleRef Mod;
  EC = LLVMParseBitcode2(MemBuf, &Mod);
  if (EC != 0) {
    fprintf(stderr, "Error parsing bitcode: %s\n", FileName);
    LLVMDisposeMemoryBuffer(MemBuf);
    shutdown(EC);
  }

  return Mod;
}
