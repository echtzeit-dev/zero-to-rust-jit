#include "zero-to-rust-jit.h"

const char *dropLeadingUnderscores(const char *Name) {
  while (Name[0] == '_')
    Name += 1;
  return Name;
}

LLVMOrcMaterializationUnitRef redirect(LLVMOrcSymbolStringPoolEntryRef Name,
                                       LLVMOrcJITTargetAddress Addr) {
  LLVMJITSymbolFlags Flags = {LLVMJITSymbolGenericFlagsWeak, 0};
  LLVMJITEvaluatedSymbol Sym = {Addr, Flags};
  LLVMOrcRetainSymbolStringPoolEntry(Name);
  LLVMOrcCSymbolMapPair Pair = {Name, Sym};
  LLVMOrcCSymbolMapPair Pairs[] = {Pair};
  return LLVMOrcAbsoluteSymbols(Pairs, 1);
}

LLVMErrorRef generator(LLVMOrcDefinitionGeneratorRef G, void *Ctx,
                       LLVMOrcLookupStateRef *LS, LLVMOrcLookupKind K,
                       LLVMOrcJITDylibRef JD, LLVMOrcJITDylibLookupFlags F,
                       LLVMOrcCLookupSet Names, size_t NamesCount) {
  ResolveFn *Resolve = (ResolveFn *)Ctx;
  for (size_t I = 0; I < NamesCount; I += 1) {
    LLVMOrcCLookupSetElement Element = Names[I];
    const char *LinkerMangled = LLVMOrcSymbolStringPoolEntryStr(Element.Name);
    const char *MangledName = dropLeadingUnderscores(LinkerMangled);

    LLVMOrcJITTargetAddress Handler = Resolve(MangledName);
    if (Handler) {
      fprintf(stderr,
              "Undefined symbol %s: redirect to host function @ 0x%016llx\n",
              MangledName, Handler);
      LLVMOrcMaterializationUnitRef MU = redirect(Element.Name, Handler);
      LLVMErrorRef Err = LLVMOrcJITDylibDefine(JD, MU);
      if (Err)
        return Err;
    }
  }

  return LLVMErrorSuccess;
}

void addGenerator(LLVMOrcJITDylibRef Unit, ResolveFn *Resolve) {
  LLVMOrcDefinitionGeneratorRef Gen =
      LLVMOrcCreateCustomCAPIDefinitionGenerator(&generator, Resolve, NULL);
  LLVMOrcJITDylibAddGenerator(Unit, Gen);
}
