//===- TailRecursionElimination.cpp - Eliminate Tail Calls ----------------===//
// Part of the Ouroboros project, under either the Apache License v2.0 with
// LLVM Exceptions or the MIT license.
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
//===----------------------------------------------------------------------===//
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
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <llvm/Analysis/InlineCost.h>
#include <llvm/IR/Analysis.h>
#include <llvm/IR/Attributes.h>
#include <variant>
using namespace llvm;

#define DEBUG_TYPE "tailrecelim"

STATISTIC(NumTailRecursionEliminatable,
          "Number of tail recursive calls that can be eliminated");

class TailCallMarker {
  Function &F;
  OptimizationRemarkEmitter *ORE;

public:
  explicit TailCallMarker(Function &F, OptimizationRemarkEmitter *ORE) noexcept
      : F(F), ORE(ORE) {}

  struct SingleCall {};
  struct MultipleCalls {
    uint8_t NumCalls;
  };
  enum class NotApplicable {
    NoCalls = 0,
    ReturnsTwice = 1,
    EmptyFunction = 2,
    NoReturnInsts = 3,
  };
  [[nodiscard]] static inline StringRef explain(NotApplicable Value) noexcept {
    switch (Value) {
    case NotApplicable::NoCalls:
      return "No tail calls found";
    case NotApplicable::ReturnsTwice:
      return "Function calls a function that returns twice";
    case NotApplicable::EmptyFunction:
      return "Function is empty";
    case NotApplicable::NoReturnInsts:
      return "Function has no return instructions";
    }
    llvm_unreachable("Invalid NotApplicable value");
  }

  using Result = std::variant<SingleCall, MultipleCalls, NotApplicable>;

  [[nodiscard]] Result markTailCalls() noexcept {
    if (F.empty()) {
      return NotApplicable::EmptyFunction;
    }

    // In the presence of `setjmp` or `longjmp`, tail call elimination is not
    // possible because the call stack frame must be preserved for non-local
    // jumps.
    if (F.callsFunctionThatReturnsTwice()) {
      return NotApplicable::ReturnsTwice;
    }

    // Functions with no return instructions cannot have tail calls
    bool hasReturn = false;
    for (const BasicBlock &BB : F) {
      if (isa<ReturnInst>(BB.getTerminator())) {
        hasReturn = true;
        break;
      }
    }
    if (!hasReturn) {
      return NotApplicable::NoReturnInsts;
    }

    // Implementation goes here
    return NotApplicable::NoCalls;
  }

  /// \brief A utility for creating an ad-hoc visitor for \c std::variant.
  ///
  /// \c VariantVisitor is a variadic template that aggregates multiple callable
  /// objects (typically lambdas) into a single functional object. This is
  /// primarily used with \c std::visit to provide a syntax similar to pattern
  /// matching in functional languages (e.g., Rust's \c match).
  ///
  /// \tparam Ts A list of base classes (lambdas or functors) to inherit from.
  template <class... Ts> struct VariantVisitor : Ts... {
    /// Bring the call operators of all base classes into the current scope.
    /// This enables the compiler to perform overload resolution across all
    /// provided callables when the visitor is invoked.
    using Ts::operator()...;
  };

  /// \brief Deduction guide for \c VariantVisitor.
  ///
  /// This guide allows the compiler to deduce the template arguments \c Ts from
  /// the constructor arguments, enabling the instantiation of the visitor
  /// without explicit template parameters.
  ///
  /// \example
  /// \code
  ///   std::variant<int, float> V = 42;
  ///   std::visit(VariantVisitor{
  ///     [](int I) { /* handle int */ },
  ///     [](float F) { /* handle float */ }
  ///   }, V);
  /// \endcode
  template <class... Ts> VariantVisitor(Ts...) -> VariantVisitor<Ts...>;
};

//===----------------------------------------------------------------------===//
// TailRecursionElimination Pass Implementation
//===----------------------------------------------------------------------===//
PreservedAnalyses
TailRecursionElimination::runOnFunction(Function &F,
                                        OptimizationRemarkEmitter &ORE) {
  if (F.getFnAttribute("disable-tail-calls").getValueAsBool()) {
    return PreservedAnalyses::all();
  }

  TailCallMarker Marker(F, &ORE);
  TailCallMarker::Result MarkerResult = Marker.markTailCalls();

  // Pattern match on analysis result with proper return handling
  return std::visit(
      TailCallMarker::VariantVisitor{
          [&](TailCallMarker::NotApplicable NA) -> PreservedAnalyses {
            LLVM_DEBUG(dbgs() << "Tail Calls Not Applicable in function "
                              << F.getName() << " because of "
                              << TailCallMarker::explain(NA) << "\n");
            // Early return - no optimization possible
            return PreservedAnalyses::all();
          },
          [&](TailCallMarker::SingleCall SC) -> PreservedAnalyses {
            LLVM_DEBUG(dbgs() << "Found single tail call in function "
                              << F.getName() << "\n");
            // TODO: Implement single tail call optimization
            llvm::errs()
                << "Single tail call optimization not yet implemented\n";
            return PreservedAnalyses::all();
          },
          [&](TailCallMarker::MultipleCalls MC) -> PreservedAnalyses {
            LLVM_DEBUG(dbgs()
                       << "Found " << static_cast<int>(MC.NumCalls)
                       << " tail calls in function " << F.getName() << "\n");
            // TODO: Implement multiple tail call optimization
            llvm::errs()
                << "Multiple tail call optimization not yet implemented\n";
            return PreservedAnalyses::all();
          }},
      MarkerResult);
}

PreservedAnalyses TailRecursionElimination::run(Function &F,
                                                FunctionAnalysisManager &AM) {
  auto &ORE = AM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  return runOnFunction(F, ORE);
}

// Forward declaration for printTailRecursionEliminationResult
static void printTailRecursionEliminationResult(llvm::raw_ostream &OutS,
                                                const StringRef TODO,
                                                const StringRef FunctionName);

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
