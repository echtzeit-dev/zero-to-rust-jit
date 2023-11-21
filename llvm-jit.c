#include "llvm-c/Core.h"
#include "llvm-c/Error.h"
#include "llvm-c/LLJIT.h"
#include "llvm-c/Support.h"
#include "llvm-c/Target.h"

#include <stdio.h>
#include <stdlib.h>

int HandleError(LLVMErrorRef Err) {
  char *ErrMsg = LLVMGetErrorMessage(Err);
  fprintf(stderr, "Error: %s\n", ErrMsg);
  LLVMDisposeErrorMessage(ErrMsg);
  return 1;
}

LLVMOrcThreadSafeModuleRef createDemoModule(void) {
  // Create a new ThreadSafeContext and underlying LLVMContext.
  LLVMOrcThreadSafeContextRef TSCtx = LLVMOrcCreateNewThreadSafeContext();

  // Get a reference to the underlying LLVMContext.
  LLVMContextRef Ctx = LLVMOrcThreadSafeContextGetContext(TSCtx);

  // Create a new LLVM module.
  LLVMModuleRef M = LLVMModuleCreateWithNameInContext("demo", Ctx);

  // Add a "sum" function":
  //  - Create the function type and function instance.
  LLVMTypeRef ParamTypes[] = {LLVMInt32Type(), LLVMInt32Type()};
  LLVMTypeRef SumFunctionType =
      LLVMFunctionType(LLVMInt32Type(), ParamTypes, 2, 0);
  LLVMValueRef SumFunction = LLVMAddFunction(M, "sum", SumFunctionType);

  //  - Add a basic block to the function.
  LLVMBasicBlockRef EntryBB = LLVMAppendBasicBlock(SumFunction, "entry");

  //  - Add an IR builder and point it at the end of the basic block.
  LLVMBuilderRef Builder = LLVMCreateBuilder();
  LLVMPositionBuilderAtEnd(Builder, EntryBB);

  //  - Get the two function arguments and use them co construct an "add"
  //    instruction.
  LLVMValueRef SumArg0 = LLVMGetParam(SumFunction, 0);
  LLVMValueRef SumArg1 = LLVMGetParam(SumFunction, 1);
  LLVMValueRef Result = LLVMBuildAdd(Builder, SumArg0, SumArg1, "result");

  //  - Build the return instruction.
  LLVMBuildRet(Builder, Result);

  //  - Free the builder.
  LLVMDisposeBuilder(Builder);

  // Our demo module is now complete. Wrap it and our ThreadSafeContext in a
  // ThreadSafeModule.
  LLVMOrcThreadSafeModuleRef TSM = LLVMOrcCreateNewThreadSafeModule(M, TSCtx);

  // Dispose of our local ThreadSafeContext value. The underlying LLVMContext
  // will be kept alive by our ThreadSafeModule, TSM.
  LLVMOrcDisposeThreadSafeContext(TSCtx);

  // Return the result.
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

static int RunDemo(LLVMOrcLLJITRef J) {
  LLVMOrcThreadSafeModuleRef TSM = createDemoModule();

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
  int ExitCode = 0;
  LLVMParseCommandLineOptions(argc, argv, "");
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();

  LLVMOrcLLJITRef J;
  LLVMErrorRef Err = LLVMOrcCreateLLJIT(&J, 0);
  if (Err) {
    ExitCode = HandleError(Err);
  } else {
    ExitCode = RunDemo(J);
    JITShutdown(J, &ExitCode);
  }

  LLVMShutdown();
  return ExitCode;
}
