/* Tomas Meszaros
 *
 * Testpass
 */

// Some relevant docs:
// https://llvm.org/doxygen/classllvm_1_1CallGraphWrapperPass.html
// https://llvm.org/doxygen/CallGraph_8h_source.html
// https://llvm.org/doxygen/classllvm_1_1CallGraphNode.html

// BOTH SEGFAULTING
// https://github.com/llvm-mirror/llvm/blob/master/tools/opt/PrintSCC.cpp
// https://github.com/mr-ma/llvm-pass-callgraph/blob/master/hello.c

// Remove function, replace all uses:
// https://groups.google.com/forum/#!topic/llvm-dev/ovvfIe_zU3Y

// Remove function from module
// https://lists.llvm.org/pipermail/llvm-dev/2012-April/049154.html

// TODO: setup input e.g. target function to reach
// TODO: check if target function is reachable

#include <string>
#include <vector>

// UndefValue
#include "llvm/IR/Constants.h"

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "testpass_config.h"

using namespace llvm;

namespace {
struct TestPass : public ModulePass {
  static char ID;
  TestPass() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

  /* Setting CallGraphWrapperPass, which will run before our pass.
     We will access output from this pass with getAnalysis<> later.
   */
  /*
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<CallGraphWrapperPass>();
  }
   */

  /* Our own stuff.
   */
  void construct_graph(StringRef root, StringRef target);
  void traverse_callgraph(void);
};
}

/* Registering our own pass, so it can be ran via opt.
 */
char TestPass::ID = 0;
static RegisterPass<TestPass> X("testpass",
                                "Just a test pass. Work in progress.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/* Debug utilities */
void debugDumpModule(Module &M) {
  if (_DEBUG) {
    errs() << ">>>>>>>>> Module dump() BEGINS:\n\n";
    M.dump();
    errs() << "<<<<<<<<< Module dump() ENDS!\n";
  }
}

void debugDumpCallGraph(CallGraph &CG) {
  if (_DEBUG) {
    errs() << ">>>>>>>>> CallGraph dump() BEGINS:\n\n";
    CG.dump();
    errs() << "<<<<<<<<< CallGraph dump() ENDS!\n";
  }
}

void debugPrint(StringRef message) {
  if (_DEBUG) {
    errs() << ">>>>>>>>> debugPrint(): " + message + "\n";
  }
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Log utulities
void logPrint(StringRef message) {
  if (_LOG) {
    errs() << "+++++++++ logPrint(): " + message + "\n";
  }
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Takes pointer to function, iterates over instruction calling this function
// and removes these instructions.
//
// Returns: number of removed instructions, -1 in case of error
int removeFunctionCalls(Function *F) {
  int calls_removed = 0;

  if (F == nullptr) {
    return -1;
  }

  for (Use &use : F->uses()) {
    if (Instruction *UserInst = dyn_cast<Instruction>(use.getUser())) {
      std::string message = "in: ";
      message += UserInst->getFunction()->getName();

      if (!UserInst->getType()->isVoidTy()) {
        // Currently only removing void returning calls.
        // TODO: handle non-void returning function calls in the future
        message += ", removing non-void returning call is not a good idea! ";
        message += "[ERROR]";
        logPrint(message);
        return -1;
      }
      message += ", removing call instruction to: ";
      message += F->getName();
      logPrint(message);

      UserInst->eraseFromParent();
      calls_removed++;
    } else {
      return -1;
    }
  }
  return calls_removed;
}

// Takes pointer to function, removes all calls to this function and then
// function itself.
//
// Returns: 0 if sucessful or -1 in case of error.
int removeFunction(Function *F) {
  if (F == nullptr) {
    return -1;
  }

  if (removeFunctionCalls(F) < 0) {
    return -1;
  }

  std::string message = "removing function: ";
  message += F->getGlobalIdentifier();
  logPrint(message);

  F->eraseFromParent();
  return 0;
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Running on each module.
bool TestPass::runOnModule(Module &M) {
  // TODO: figure out how to automatically extract needed path to target
  debugDumpModule(M);
  Function *x_fcn = nullptr;
  for (Function &F : M) {
    if (F.getGlobalIdentifier() == "x") {
      x_fcn = &F;
    }
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        continue;
      }
    }
  }
  removeFunction(x_fcn);
  debugDumpModule(M);
  return true;
}

void TestPass::traverse_callgraph(void) {
  CallGraph &CallGraph = getAnalysis<CallGraphWrapperPass>().getCallGraph();
  debugDumpCallGraph(CallGraph);

  for (auto &node : CallGraph) {
    auto *graph_node = node.second.get();

    /* NOTE:
       I don't know if CallGraph node can be null, but putting this in here just
       in case.
     */
    if (nullptr == graph_node) {
      if (_DEBUG) {
        errs() << ">>> graph_node is NULL <<<\n\n";
      }
      continue;
    }

    /* Indirect calls can cause function within the call graph node to be null.
       Need to check for this or there will be segfault.

       This is going to fail:

          auto fcn = graph_node->getFunction();
          auto fcn = node.first();

       Need to first check:

          node.first == nullptr
     */
    auto *fcn_const = node.first;
    if (nullptr == fcn_const) {
      if (_DEBUG) {
        errs() << ">>> fnc is NULL <<<\n\n";
      }
      continue;
    }
  }

  debugDumpCallGraph(CallGraph);
}