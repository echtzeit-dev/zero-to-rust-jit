#include "llvm-c/BitReader.h"
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/LLJIT.h"
#include "llvm-c/Support.h"
#include "llvm-c/Target.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int HandleError(LLVMErrorRef Err) {
  char *ErrMsg = LLVMGetErrorMessage(Err);
  fprintf(stderr, "Error: %s\n", ErrMsg);
  LLVMDisposeErrorMessage(ErrMsg);
  return 1;
}

static void DiagnosticHandler(LLVMDiagnosticInfoRef DI, void *C) {
  char *CErr = LLVMGetDiagInfoDescription(DI);
  fprintf(stderr, "Error with new bitcode parser: %s\n", CErr);
  LLVMDisposeMessage(CErr);
  exit(EXIT_FAILURE);
}

LLVMOrcThreadSafeModuleRef CreateDemoModule(const char *FileName) {
  LLVMBool EC;

  char *Err;
  LLVMMemoryBufferRef MemBuf;
  EC = LLVMCreateMemoryBufferWithContentsOfFile(FileName, &MemBuf, &Err);
  if (EC != 0) {
    fprintf(stderr, "Error reading file %s: %s\n", FileName, Err);
    return NULL;
  }

  // Orc supports multi-threaded compilation and execution, so it needs a
  // thread-safe context.
  LLVMOrcThreadSafeContextRef TSCtx = LLVMOrcCreateNewThreadSafeContext();
  LLVMContextRef Ctx = LLVMOrcThreadSafeContextGetContext(TSCtx);
  LLVMContextSetDiagnosticHandler(Ctx, DiagnosticHandler, NULL);

  LLVMModuleRef M;
  EC = LLVMParseBitcode2(MemBuf, &M);
  if (EC != 0) {
    fprintf(stderr, "Parsing bitcode from file %s failed with error code: %d\n",
            "demo_sum.ll", EC);
    LLVMDisposeMemoryBuffer(MemBuf);
    return NULL;
  }

  // Return single entity that bundles ownership of module and context.
  LLVMOrcThreadSafeModuleRef TSM = LLVMOrcCreateNewThreadSafeModule(M, TSCtx);
  LLVMOrcDisposeThreadSafeContext(TSCtx);
  return TSM;
}

static void JITShutdown(LLVMOrcLLJITRef J, int *ExitCode) {
  // Destroy our JIT instance. This will clean up any memory that the JIT has
  // taken ownership of. This operation is non-trivial (e.g. it may need to
  // JIT static destructors) and may also fail. In that case we want to render
  // the error to stderr, but not overwrite any existing return value.
  LLVMErrorRef Err = LLVMOrcDisposeLLJIT(J);
  if (Err) {
    int NewFailureResult = HandleError(Err);
    if (*ExitCode == 0)
      *ExitCode = NewFailureResult;
  }
}

static int RunDemo(LLVMOrcLLJITRef J, const char *FileName) {
  LLVMOrcThreadSafeModuleRef TSM = CreateDemoModule(FileName);
  if (TSM == NULL)
    return EXIT_FAILURE;

  LLVMOrcJITDylibRef MainJD = LLVMOrcLLJITGetMainJITDylib(J);
  LLVMErrorRef Err = LLVMOrcLLJITAddLLVMIRModule(J, MainJD, TSM);
  if (Err) {
    // If adding the ThreadSafeModule fails then we need to clean it up
    // ourselves. If adding it succeeds the JIT will manage the memory.
    LLVMOrcDisposeThreadSafeModule(TSM);
    return HandleError(Err);
  }

  LLVMOrcJITTargetAddress SumAddr;
  Err = LLVMOrcLLJITLookup(J, &SumAddr, "sum");
  if (Err)
    return HandleError(Err);

  // If we made it here then everything succeeded. Execute our JIT'd code.
  int32_t (*Sum)(int32_t, int32_t) = (int32_t(*)(int32_t, int32_t))SumAddr;
  int32_t Result = Sum(1, 2);

  printf("1 + 2 = %i\n", Result);
  return EXIT_SUCCESS;
}

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    const char *BaseName = strrchr(argv[0], '/');
    const char *ExecName = BaseName ? BaseName + 1 : "llvm-jit-c";
    fprintf(stderr, "Usage: %s bitcode\n", ExecName);
    return EXIT_FAILURE;
  }

  int ExitCode = 0;
  LLVMParseCommandLineOptions(argc, argv, "");
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();

  LLVMOrcLLJITRef J;
  LLVMErrorRef Err = LLVMOrcCreateLLJIT(&J, 0);
  if (Err) {
    ExitCode = HandleError(Err);
  } else {
    ExitCode = RunDemo(J, argv[1]);
    JITShutdown(J, &ExitCode);
  }

  LLVMShutdown();
  return ExitCode;
}
