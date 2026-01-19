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
#include "llvm/Support/raw_ostream.h"
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/CaptureTracking.h>
#include <llvm/Analysis/InlineCost.h>
#include <llvm/IR/Analysis.h>
#include <llvm/IR/Attributes.h>
#include <llvm/IR/Instruction.h>
using namespace llvm;

#define DEBUG_TYPE "tailrecelim"

STATISTIC(NumAllocaUsers, "Number of alloca users tracked");
STATISTIC(NumEscapedInstructions, "Number of escaped instructions tracked");
STATISTIC(NumCallsMarkedWithTailAttr,
          "Number of calls marked with the `tail` attribute");

namespace {
class EscapeUsersAnalysis {
public:
  EscapeUsersAnalysis(const Function &F, OptimizationRemarkEmitter *const ORE,
                      AliasAnalysis *const AA) noexcept
      : F(F), ORE(ORE), AA(AA) {}

  /// \brief Perform a specialized escape analysis for stack-allocated values.
  ///
  /// This method orchestrates the discovery of all local memory roots (allocas
  /// and byval arguments) and tracks their usage through the function.
  void trackAll() noexcept {
    // The `byval` arguments are held by the local stack frame.
    // We need to track both where they escape (or are captured) and where
    // they are used.
    //
    // We are not interested in other kinds of arguments, such as `byref`,
    // since they are not allocated on the local stack frame.
    // If an argument is not `byval`, the caller is responsible for the
    // argument's lifetime --- it is either allocated on the caller's stack
    // frame or on the heap. Thus, we do not run the risk of overwriting
    // the argument's value during a tail-call stack reuse.
    //
    // Note: Return values are not tracked as they are either passed via
    // registers or handled by the caller.
    for (const Argument &Arg : F.args()) {
      if (Arg.hasByValAttr())
        track(&Arg);
    }
    // The `alloca` instructions are held by the local stack frame.
    // We track their transitive use-def chains to identify potential
    // memory corruption during stack frame reuse.
    for (auto &BB : F) {
      for (auto &I : BB)
        if (const AllocaInst *AI = dyn_cast<AllocaInst>(&I))
          track(AI);
    }

    NumAllocaUsers += AllocaCallUsers.size();
    NumEscapedInstructions += EscapedInstructions.size();
  }

private:
  /// \brief Trace the use-def chain of a stack-root value.
  ///
  /// This initiates a Pointer Tracking phase to populate the AllocaUsers
  /// and EscapedInstructions sets. Formally, this serves as an Abstract
  /// Dataflow Integrity Check to ensure that stack frame reuse does not
  /// violate memory safety for any instructions relying on local addresses.
  void track(const Value *V) noexcept {
    SmallVector<const Use *, 32> Worklist;
    SmallPtrSet<const Use *, 32> Visited;

    auto AddUsesToWorklist = [&](const Value *V) {
      for (auto &U : V->uses()) {
        if (!Visited.insert(&U).second)
          continue;
        Worklist.push_back(&U);
      }
    };

    // Initialize the worklist with the uses of the given value.
    AddUsesToWorklist(V);

    while (!Worklist.empty()) {
      const Use *U = Worklist.pop_back_val();
      Instruction *I = cast<Instruction>(U->getUser());

      switch (I->getOpcode()) {
      case Instruction::Call:
      case Instruction::Invoke: {
        auto &CB = cast<CallBase>(*I);

        // A 'byval' argument is not an escape point because the backend
        // guarantees a bitwise copy of the data. The callee receives its
        // own private copy on the stack, so it cannot access or capture
        // the original alloca's address.
        if (CB.isArgOperand(U) && CB.isByValArgument(CB.getArgOperandNo(U)))
          continue;

        // Register this call/invoke as a reachability boundary.
        // Any function receiving a local pointer is a potential candidate
        // for stack frame interference during Tail Call Optimization.
        AllocaCallUsers.insert(&CB);

        // Check for 'nocapture' attribute: this is a formal guarantee that
        // the callee will not store the pointer in a location that outlives
        // the call itself (e.g., globals or heap).
        bool IsNocapture =
            CB.isDataOperand(U) && CB.doesNotCapture(CB.getDataOperandNo(U));

        if (IsNocapture)
          continue;

        // Even without 'nocapture', a call only escapes the pointer if it
        // has side effects on memory. If the function is 'readonly' or
        // 'readnone', it cannot leak the address to persistent storage.
        if (!CB.onlyReadsMemory())
          EscapedInstructions.insert(&CB);

        break;
      }
      case Instruction::Load: {
        // The result of a load is not alloca-derived, unless it's a load from
        // an alloca that has otherwise escaped, but this is a local analysis.
        // The escape point is already/will be tracked by the other
        // cases if the alloca has escaped.
        continue;
      }
      case Instruction::Store: {
        if (U->getOperandNo() == 0)
          EscapedInstructions.insert(I);
        // Stores have no users to analyze because they don't produce a value.
        continue;
      }
      case Instruction::BitCast:
      case Instruction::GetElementPtr:
      case Instruction::PHI:
      case Instruction::Select:
      case Instruction::AddrSpaceCast:
        // TODO: Consider to add other Instructions
        break;
      default:
        EscapedInstructions.insert(I);
        break;
      }
      AddUsesToWorklist(I);
    }
  }

  /// \brief Identify the set of call sites that access the local stack frame.
  ///
  /// This set represents the "External Reachability Frontier" of alloca
  /// instructions. It contains all 'call' and 'invoke' instructions that
  /// receive a pointer derived from a local stack root as an argument.
  ///
  /// Formally, this is a filtered transitive closure of the use-def chain,
  /// capturing only the boundaries where local memory is passed to callees.
  SmallPtrSet<Instruction *, 32> AllocaCallUsers;
  /// \brief A set of instructions where the local stack pointer escapes.
  ///
  /// Once a local pointer is captured or stored in a way that escapes the
  /// local scope, it is considered an escape point, and the stack frame
  /// can no longer be safely optimized for tail calls.
  SmallPtrSet<Instruction *, 32> EscapedInstructions;

  const Function &F;
  OptimizationRemarkEmitter *const ORE;
  AliasAnalysis *const AA;
};

class TailCallMarker {
  const Function &F;
  [[maybe_unused]] OptimizationRemarkEmitter *const ORE;
  [[maybe_unused]] AliasAnalysis *const AA;

public:
  explicit TailCallMarker(const Function &F,
                          OptimizationRemarkEmitter *const ORE,
                          AliasAnalysis *const AA) noexcept
      : F(F), ORE(ORE), AA(AA) {}

  /// \brief Mark tail calls within the function and perform escape analysis.
  ///
  /// This method scans the function to identify calls that can be safely
  /// marked with the 'tail' attribute.
  ///
  /// Note: LLVM's definition of a 'tail' call differs from the standard
  /// functional programming definition. In LLVM, a 'tail' marker is a
  /// guarantee that the callee does not access the caller's stack frame. This
  /// allows the Backend to perform Sibling Call Optimization even for calls
  /// that are not formally at the end of the function.
  ///
  /// For instance, the attribute set here is consumed by:
  /// - SelectionDAGBuilder::LowerCallTo (SelectionDAGBuilder.cpp): Which
  ///   translates the IR attribute into a 'isTailCall' flag for the CodeGen.
  /// - X86TargetLowering::LowerCall (X86ISelLowering.cpp): Which performs the
  ///   final architecture-specific eligibility check via
  ///   IsEligibleForTailCallOptimization.
  ///
  /// \returns true if any calls were marked or any changes were made to the
  /// IR.
  [[nodiscard]] bool markTailCalls() noexcept {
    NumCallsMarkedWithTailAttr = 0;
    bool MadeChanges = false;
    if (F.empty()) {
      return MadeChanges;
    }
    // In the presence of `setjmp` or `longjmp`, tail call elimination is not
    // possible because the call stack frame must be preserved for non-local
    // jumps.
    if (F.callsFunctionThatReturnsTwice()) {
      return MadeChanges;
    }

    EscapeUsersAnalysis EscapeUsersAnalysis(F, ORE, AA);
    EscapeUsersAnalysis.trackAll();

    return MadeChanges;
  }
};
}; // namespace

//===----------------------------------------------------------------------===//
// TailRecursionElimination Pass Implementation
//===----------------------------------------------------------------------===//
PreservedAnalyses TailRecursionElimination::runOnFunction(
    Function &F, OptimizationRemarkEmitter &ORE, AliasAnalysis &AA) {
  if (F.getFnAttribute("disable-tail-calls").getValueAsBool()) {
    return PreservedAnalyses::all();
  }

  bool MadeChange = false;

  TailCallMarker Marker(F, &ORE, &AA);
  MadeChange |= Marker.markTailCalls();

  if (!MadeChange)
    return PreservedAnalyses::all();
}

PreservedAnalyses TailRecursionElimination::run(Function &F,
                                                FunctionAnalysisManager &AM) {
  OptimizationRemarkEmitter &ORE =
      AM.getResult<OptimizationRemarkEmitterAnalysis>(F);
  AliasAnalysis &AA = AM.getResult<AAManager>(F);
  return runOnFunction(F, ORE, AA);
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
