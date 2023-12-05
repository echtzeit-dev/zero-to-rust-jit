#include "zero-to-rust-jit.h"

LLVMModuleRef buildModule(LLVMOrcThreadSafeContextRef Ctx) {
  LLVMContextRef BareCtx = LLVMOrcThreadSafeContextGetContext(Ctx);
  LLVMModuleRef M = LLVMModuleCreateWithNameInContext("demo", BareCtx);

  // Create the function type and function instance
  LLVMTypeRef ParamTypes[] = {LLVMInt32Type(), LLVMInt32Type()};
  LLVMTypeRef SumFunctionType =
      LLVMFunctionType(LLVMInt32Type(), ParamTypes, 2, 0);
  LLVMValueRef SumFunction = LLVMAddFunction(M, "sum", SumFunctionType);

  // Add a basic block to the function
  LLVMBasicBlockRef EntryBB = LLVMAppendBasicBlock(SumFunction, "entry");

  // Create the IR builder and point it at the end of the basic block
  LLVMBuilderRef Builder = LLVMCreateBuilder();
  LLVMPositionBuilderAtEnd(Builder, EntryBB);

  // Insert an instruction that adds the input parameters
  LLVMValueRef SumArg0 = LLVMGetParam(SumFunction, 0);
  LLVMValueRef SumArg1 = LLVMGetParam(SumFunction, 1);
  LLVMValueRef Result = LLVMBuildAdd(Builder, SumArg0, SumArg1, "result");

  // Insert the return instruction
  LLVMBuildRet(Builder, Result);

  // Free the IR builder
  LLVMDisposeBuilder(Builder);
  return M;
}
