// Some relevant docs:
// https://llvm.org/doxygen/classllvm_1_1CallGraphWrapperPass.html
// https://llvm.org/doxygen/CallGraph_8h_source.html
// https://llvm.org/doxygen/classllvm_1_1CallGraphNode.html

#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/Analysis/CallGraph.h"

using namespace llvm;

namespace {
struct TestPass : public ModulePass {
  static char ID;

  TestPass() : ModulePass(ID) {}

  // We want to run CallGraphWrapperPass before we run this pass, so we can get
  // call graph and work with it.
  void getAnalysisUsage(AnalysisUsage &AU) const {
    // AU.setPreservesCFG();
    AU.addRequired<CallGraphWrapperPass>();
  }

  virtual bool runOnModule(Module &M) {
    CallGraph &call_graph = getAnalysis<CallGraphWrapperPass>().getCallGraph();
    StringRef target_fcn_name = "x";

    for (auto const &F : M) {
      // This should be fine for global lookup.
      StringRef id = F.getGlobalIdentifier();

      /*
      for (auto &B : F) {
        for (auto &I : B) {
          // Check if we are on the call instruction. If yes, get it.
          if (auto callinst = dyn_cast<CallInst>(&I)) {
            Function *called_fcn = callinst->getCalledFunction();

            // TODO: make this automatic
            if (called_fcn->getName() == target_fcn_name) {
              // Collect desired instructions to be removed.
            }
          }
        }
      }
       */
    }

    // We cannot remove instructions inside for loop, need to do it here.
    for (auto &node : call_graph) {
      CallGraphNode *_node = node.second.get();
      final(_node);
    }
    return false;
  }

  void final(CallGraphNode *node) {
    errs() << "<<< Final stuff\n";
    Function *fcn = node->getFunction();
    // FIXME: segfaults
    errs() << fcn->getName();
    errs() << ">>> Final stuff\n";
  }
};
}

char TestPass::ID = 0;
static RegisterPass<TestPass> X("testpass", "Just a test pass.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);
