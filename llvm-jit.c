#include "llvm-c/BitReader.h"
#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/LLJIT.h"
#include "llvm-c/Support.h"
#include "llvm-c/Target.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdnoreturn.h>

static int HandleError(LLVMErrorRef Err) {
  char *ErrMsg = LLVMGetErrorMessage(Err);
  fprintf(stderr, "Error: %s\n", ErrMsg);
  LLVMDisposeErrorMessage(ErrMsg);
  return 1;
}

static noreturn void DiagnosticHandler(LLVMDiagnosticInfoRef DI, void *C) {
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

LLVMOrcLLJITRef J;

static noreturn void Halt() {
  fprintf(stderr, "Encountered undefined function. Halting.\n");
  while (true)
    ;
}

static noreturn void RustPanicHandler(const char *str) {
  fprintf(stderr, "Panic due to overflow: %s\n", str);
  int ExitCode = 0;
  JITShutdown(J, &ExitCode);
  // Indicate default failure if JIT shutdown didn't fail
  if (ExitCode == 0)
    ExitCode = EXIT_FAILURE;
  LLVMShutdown();
  exit(ExitCode);
}

static const char *DropLeadingUnderscores(const char *Name) {
  while (Name[0] == '_') {
    assert(Name[0] != '\0' && "No pure-underscore function names please");
    Name += 1;
  }
  return Name;
}

static bool IsRustPanicHandler(const char *MangledName) {
  const char *CmpName = DropLeadingUnderscores(MangledName);
  const char *PanicHandlerSig = "ZN4core9panicking";
  return strncmp(CmpName, PanicHandlerSig, strlen(PanicHandlerSig)) == 0;
}

static LLVMOrcMaterializationUnitRef
HostFunctionRedirect(LLVMOrcSymbolStringPoolEntryRef Name,
                     LLVMOrcJITTargetAddress Addr) {
  LLVMJITSymbolFlags Flags = {LLVMJITSymbolGenericFlagsWeak, 0};
  LLVMJITEvaluatedSymbol Sym = {Addr, Flags};
  LLVMOrcRetainSymbolStringPoolEntry(Name);
  LLVMOrcCSymbolMapPair Pair = {Name, Sym};
  LLVMOrcCSymbolMapPair Pairs[] = {Pair};
  return LLVMOrcAbsoluteSymbols(Pairs, 1);
}

#define STRINGIFY(a) STRINGIFY_DETAIL(a)
#define STRINGIFY_DETAIL(a) #a

// Stub definition generator, where all Names are materialized from the
// materializationUnitFn() test function and defined into the JIT Dylib
static LLVMErrorRef
JITDefinitionGeneratorFn(LLVMOrcDefinitionGeneratorRef G, void *Ctx,
                         LLVMOrcLookupStateRef *LS, LLVMOrcLookupKind K,
                         LLVMOrcJITDylibRef JD, LLVMOrcJITDylibLookupFlags F,
                         LLVMOrcCLookupSet Names, size_t NamesCount) {
  for (size_t I = 0; I < NamesCount; I += 1) {
    LLVMOrcCLookupSetElement Element = Names[I];
    LLVMOrcJITTargetAddress Handler;

    const char *MangledName = LLVMOrcSymbolStringPoolEntryStr(Element.Name);
    if (IsRustPanicHandler(MangledName)) {
      Handler = (LLVMOrcJITTargetAddress)&RustPanicHandler;
      fprintf(stderr, "Redirecting to host function %s @ 0x%016llx: %s\n",
              STRINGIFY(RustPanicHandler), Handler, MangledName);
      LLVMOrcMaterializationUnitRef MU =
          HostFunctionRedirect(Element.Name, Handler);
      LLVMErrorRef Err = LLVMOrcJITDylibDefine(JD, MU);
      if (Err)
        return Err;
    }

    // Other underfined functions will be reported as regular lookup errors
  }
  return LLVMErrorSuccess;
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

  LLVMOrcDefinitionGeneratorRef Gen =
      LLVMOrcCreateCustomCAPIDefinitionGenerator(&JITDefinitionGeneratorFn,
                                                 NULL, NULL);
  LLVMOrcJITDylibAddGenerator(MainJD, Gen);

  LLVMOrcJITTargetAddress SumAddr;
  Err = LLVMOrcLLJITLookup(J, &SumAddr, "sum");
  if (Err)
    return HandleError(Err);

  // If we made it here then everything succeeded. Execute our JIT'd code.
  int32_t (*Sum)(int32_t, int32_t) = (int32_t(*)(int32_t, int32_t))SumAddr;
  int32_t Result = Sum(0x80000000, 0x80000000);

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
