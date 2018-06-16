// Some relevant docs:
// https://llvm.org/doxygen/classllvm_1_1CallGraphWrapperPass.html
// https://llvm.org/doxygen/CallGraph_8h_source.html
// https://llvm.org/doxygen/classllvm_1_1CallGraphNode.html

// BOTH SEGFAULTING
// https://github.com/llvm-mirror/llvm/blob/master/tools/opt/PrintSCC.cpp
// https://github.com/mr-ma/llvm-pass-callgraph/blob/master/hello.c

#include <vector>

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "testpass_config.h"

using namespace llvm;

namespace {
struct TestPass : public ModulePass {
  static char ID;
  TestPass() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<CallGraphWrapperPass>();
  }
};
}
char TestPass::ID = 0;
static RegisterPass<TestPass> X("testpass", "Just a test pass.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);

bool TestPass::runOnModule(Module &M) {
  CallGraph &CallGraph = getAnalysis<CallGraphWrapperPass>().getCallGraph();

  for (auto &node : CallGraph) {
    auto graph_node = node.second.get();

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

    if (_DEBUG) {
      graph_node->dump();
    }

    /* Indirect calls can cause function within the call graph node to be null.
       Need to check for this or there will be segfault.

       This is going to fail:

          auto fcn = graph_node->getFunction();
          auto fcn = node.first();

       Need to first check:

          node.first == nullptr
     */
    auto fcn = node.first;
    if (nullptr == fcn) {
      if (_DEBUG) {
        errs() << ">>> fnc is NULL <<<\n\n";
        errs() << "======\n";
      }
      continue;
    }

    if (_DEBUG) {
      errs() << ">> fcn: <<<\n";
      fcn->dump();
      errs() << "======\n";
    }
  }

  return true;
}