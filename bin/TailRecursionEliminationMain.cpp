//========================================================================
// FILE:
//    TailRecursionEliminationMain.cpp
//
// DESCRIPTION:
//    TODO
//    Internally it uses the TailRecursionElimination pass.
//
// USAGE:
//    # First, generate an LLVM file:
//      clang -emit-llvm <input-file> -c -o <output-llvm-file>
//    # Now you can run this tool as follows:
//      <BUILD/DIR>/bin/tre <output-llvm-file>
//========================================================================
#include "TailRecursionElimination.h"

#include "llvm/IRReader/IRReader.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
// Command line options
//===----------------------------------------------------------------------===//
static cl::OptionCategory TailRecursionEliminationCategory{
    "tail-recursion-elimination options"};

static cl::opt<std::string> InputModule{
    cl::Positional,
    cl::desc{"<Module to analyze>"},
    cl::value_desc{"bitcode filename"},
    cl::init(""),
    cl::Required,
    cl::cat{TailRecursionEliminationCategory}};

//===----------------------------------------------------------------------===//
// tre - implementation
//===----------------------------------------------------------------------===//
static void runTailRecursionElimination(Module &M) {
  // Create a module pass manager and add our passes to it.
  FunctionPassManager FPM;
  FPM.addPass(TailRecursionElimination());
  FPM.addPass(TailRecursionEliminationPrinter(llvm::errs()));

  // Create an empty module analysis manager
  FunctionAnalysisManager FAM;

  // Register all available module analysis passes defined in PassRegistry.def.
  // We only really need PassInstrumentationAnalysis (which is pulled by
  // default by PassBuilder), but to keep this concise, let PassBuilder do all
  // the _heavy-lifting_.
  PassBuilder PB;
  PB.registerFunctionAnalyses(FAM);

  // Finally, run our passes on the module.
  for (Function &F : M)
    FPM.run(F, FAM);
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
int main(int Argc, char **Argv) {
  // Hide all options apart from the ones specific to this tool
  cl::HideUnrelatedOptions(TailRecursionEliminationCategory);

  cl::ParseCommandLineOptions(Argc, Argv,
                              "Counts the number of function "
                              "in the input IR file\n");

  // Makes sure llvm_shutdown() is called (which cleans up LLVM objects)
  //  http://llvm.org/docs/ProgrammersManual.html#ending-execution-with-llvm-shutdown
  llvm_shutdown_obj SDO;

  // Parse the IR file passed on the command line.
  SMDiagnostic Err;
  LLVMContext Ctx;
  std::unique_ptr<Module> M = parseIRFile(InputModule.getValue(), Err, Ctx);

  if (!M) {
    errs() << "Error reading bitcode file: " << InputModule << "\n";
    Err.print(Argv[0], errs());
    return -1;
  }

  // Run the analysis and print the results
  runTailRecursionElimination(*M);

  return 0;
}
