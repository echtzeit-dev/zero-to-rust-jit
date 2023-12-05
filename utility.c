#include "zero-to-rust-jit.h"

LLVMOrcLLJITRef JitInst = NULL;
LLVMOrcThreadSafeContextRef CtxInst = NULL;

int handleError(LLVMErrorRef Err) {
  char *ErrMsg = LLVMGetErrorMessage(Err);
  fprintf(stderr, "Error: %s\n", ErrMsg);
  LLVMDisposeErrorMessage(ErrMsg);
  return 1;
}

void init(int argc, const char *argv[], LLVMOrcLLJITRef *Jit,
          LLVMOrcThreadSafeContextRef *Ctx) {
  LLVMParseCommandLineOptions(argc, argv, "");
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();

  // Keep local copies for simplicity (it's all opaque pointers)
  LLVMErrorRef Err = LLVMOrcCreateLLJIT(&JitInst, 0);
  if (Err)
    shutdown(handleError(Err)); // noreturn
  CtxInst = LLVMOrcCreateNewThreadSafeContext();

  *Jit = JitInst;
  *Ctx = CtxInst;
}

void addModule(LLVMOrcLLJITRef Jit, LLVMModuleRef Mod) {
  LLVMOrcThreadSafeModuleRef TSM =
      LLVMOrcCreateNewThreadSafeModule(Mod, CtxInst);
  LLVMOrcJITDylibRef MainJD = LLVMOrcLLJITGetMainJITDylib(Jit);

  // If adding the ThreadSafeModule fails then we need to clean it up
  // ourselves. If adding it succeeds the JIT will manage the memory.
  LLVMErrorRef Err = LLVMOrcLLJITAddLLVMIRModule(Jit, MainJD, TSM);
  if (Err) {
    LLVMOrcDisposeThreadSafeModule(TSM);
    shutdown(handleError(Err)); // noreturn
  }
}

void loop(int (*Sum)(int, int)) {
  char answer;
  do {
    int a, b;
    printf("a = ");
    if (scanf("%x", &a) != 1)
      shutdown(EXIT_FAILURE);

    printf("b = ");
    if (scanf("%x", &b) != 1)
      shutdown(EXIT_FAILURE);

    int Result = Sum(a, b);
    printf("%i + %i = %i\n", a, b, Result);

    printf("Again? (y/n) ");
    if (scanf(" %c", &answer) != 1)
      shutdown(EXIT_FAILURE);

    printf("\n");
  } while (answer == 'y' || answer == 'Y');
}

void shutdown(int ExitCode) {
  if (JitInst) {
    // Destroy our JIT instance. This will clean up any memory that the JIT has
    // taken ownership of. This operation is non-trivial (e.g. it may need to
    // JIT static destructors) and may also fail. In that case we want to render
    // the error to stderr, but not overwrite any existing return value.
    LLVMErrorRef Err = LLVMOrcDisposeLLJIT(JitInst);
    if (Err) {
      int JITShutdownExitCode = handleError(Err);
      if (ExitCode == 0)
        ExitCode = JITShutdownExitCode;
    }
  }

  if (CtxInst) {
    LLVMOrcDisposeThreadSafeContext(CtxInst);
  }

  LLVMShutdown();
  exit(ExitCode);
}
