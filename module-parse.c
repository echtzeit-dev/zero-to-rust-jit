#include "zero-to-rust-jit.h"

LLVMModuleRef parseModule(const char *IRCode, LLVMOrcThreadSafeContextRef Ctx) {
  LLVMContextRef BareCtx = LLVMOrcThreadSafeContextGetContext(Ctx);
  LLVMMemoryBufferRef Buf = LLVMCreateMemoryBufferWithMemoryRange(
      IRCode, strlen(IRCode), "demo", /*RequiresNullTerminator*/ 1);

  char *Err;
  LLVMModuleRef Mod;
  LLVMBool EC = LLVMParseIRInContext(BareCtx, Buf, &Mod, &Err);
  if (EC != 0) {
    fprintf(stderr, "Error parsing IR-code: %s\n", Err);
    shutdown(EC);
  }

  return Mod;
}
