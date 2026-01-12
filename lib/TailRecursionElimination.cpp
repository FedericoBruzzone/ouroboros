//==============================================================================
// FILE:
//    TailRecursionElimination.cpp
//
// DESCRIPTION:
//    TODO
//
//    This pass is used in `tre`, a tool implemented in
//    tools/TailRecursionEliminationMain.cpp that is a wrapper around
//    TailRecursionElimination. `tre` allows you to run
//    TailRecursionEliminationwithout `opt`.
//
// USAGE:
//     1. Directly with `opt`:
//      opt -load-pass-plugin libTailRecursionElimination.dylib `\`
//        -passes="print<tailrecelim>" `\`
//        -disable-output <input-llvm-file>
//     2. Through an optimisation pipeline:
//      opt -load-pass-plugin libTailRecursionElimination.dylib `\`
//        --passes='default<O1>' `\`
//        -disable-output <input-llvm-file>
//
//==============================================================================
#include "TailRecursionElimination.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

#define DEBUG_TYPE "tailrecelim"

STATISTIC(NumTailRecursionEliminatable,
          "Number of tail recursive calls that can be eliminated");

// Pretty-prints the result
static void printTailRecursionEliminationResult(llvm::raw_ostream &OutS,
                                                const StringRef TODO,
                                                const StringRef FunctionName);

//------------------------------------------------------------------------------
// TailRecursionElimination Implementation
//------------------------------------------------------------------------------
PreservedAnalyses TailRecursionElimination::runOnFunction(Function &F) {
  llvm::errs() << "Unimplemented\n";
  NumTailRecursionEliminatable = 0;
  return PreservedAnalyses::all();
}

PreservedAnalyses TailRecursionElimination::run(Function &F,
                                                FunctionAnalysisManager &) {
  return runOnFunction(F);
}

PreservedAnalyses
TailRecursionEliminationPrinter::run(Function &F, FunctionAnalysisManager &AM) {
  printTailRecursionEliminationResult(OS, "TODO", F.getName());
  return PreservedAnalyses::all();
}

//------------------------------------------------------------------------------
// New PM Registration
//------------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getTailRecursionEliminationPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "tailrecelim", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // #1 REGISTRATION FOR "opt -passes=tailrecelim"
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "tailrecelim") {
                    FPM.addPass(TailRecursionElimination());
                    return true;
                  }
                  return false;
                });

            // #2 REGISTRATION FOR "opt -passes=print<tailrecelim>"
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<tailrecelim>") {
                    FPM.addPass(TailRecursionEliminationPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });

            // #3 REGISTRATION FOR "-O{1,2,3,s,z}" default pipelines
            // Register the pass as a step of an existing pipeline.
            // The insertion point here is at the start of the function
            // pipeline.
            //
            // To be more precise, using this callback means that
            // the TailRecursionElimination pass will be added to
            // the function pipeline whenever any of the
            // "-O{1,2,3,s,z}" optimization levels are used.
            //
            // More info: https://llvm.org/docs/NewPassManager.html
            // PB.registerVectorizerStartEPCallback(
            //     [](FunctionPassManager &FPM, OptimizationLevel Level) {
            //       FPM.addPass(TailRecursionElimination());
            //     });
          }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getTailRecursionEliminationPluginInfo();
}

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
static void printTailRecursionEliminationResult(raw_ostream &OutS,
                                                const StringRef TODO,
                                                const StringRef FunctionName) {
  // The following is visible only if you pass -debug on the command line
  // and you have an assert build.
  // errs() is an alternative to dbgs()
  LLVM_DEBUG(dbgs() << "===== DEBUG EXAMPLE =====\n");

  OutS << "Tail Recursion Elimination Result:\n";
  OutS << "  Function: " << FunctionName << "\n";
  OutS << "  TODO: " << TODO << "\n";

  // Uncomment to flush the output stream immediately
  // OutS.flush();
}
