//========================================================================
// FILE:
//    TailRecursionElimination.h
//
// DESCRIPTION:
//    Declares the TailRecursionElimination pass
//      * new pass manager interface
//      * legacy pass manager interface
//      * printer pass for the new pass manager
//========================================================================

#ifndef TAIL_RECURSION_ELIMINATION_H
#define TAIL_RECURSION_ELIMINATION_H
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

class Function;

class TailRecursionElimination
    : public PassInfoMixin<TailRecursionElimination> {
public:
  TailRecursionElimination() = default;
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
  PreservedAnalyses runOnFunction(Function &F);
};

class TailRecursionEliminationPrinter
    : public PassInfoMixin<TailRecursionEliminationPrinter> {
public:
  explicit TailRecursionEliminationPrinter(llvm::raw_ostream &OutS)
      : OS(OutS) {}
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);

  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;
};

} // namespace llvm

#endif // TAIL_RECURSION_ELIMINATION_H
