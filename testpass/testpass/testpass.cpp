// Some relevant docs:
// https://llvm.org/doxygen/classllvm_1_1CallGraphWrapperPass.html
// https://llvm.org/doxygen/CallGraph_8h_source.html
// https://llvm.org/doxygen/classllvm_1_1CallGraphNode.html

// BOTH SEGFAULTING
// https://github.com/llvm-mirror/llvm/blob/master/tools/opt/PrintSCC.cpp
// https://github.com/mr-ma/llvm-pass-callgraph/blob/master/hello.c

#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "llvm/ADT/SCCIterator.h"
#include "llvm/Analysis/CallGraph.h"

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
  for (auto &_node : CallGraph) {
    auto node = _node.second.get();
    Function *func = node->getFunction();

    // SEGFAULTS HERE
    auto id = func->getGlobalIdentifier();
  }

  return true;
}