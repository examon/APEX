#include <map>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct TestPass : public FunctionPass {
  static char ID;
  TestPass() : FunctionPass(ID) {}

  std::map<StringRef, std::vector<Function *>> id_calls;
  std::vector<Instruction *> instruction_to_erase;

  virtual bool runOnFunction(Function &F) {
    // This should be fine for global lookup.
    StringRef id = F.getGlobalIdentifier();

    id_calls[id] = std::vector<Function *>();
    errs() << "\nin fcn: " << F.getName() << "\n";
    for (auto &B : F) {
      for (auto &I : B) {
        // Check if we are on the call instruction. If yes, get it.
        if (auto callinst = dyn_cast<CallInst>(&I)) {
          Function *called_fcn = callinst->getCalledFunction();
          id_calls[id].push_back(called_fcn);
          I.dump();

          if (called_fcn->getName() == "x") {
            errs() << "should remove here\n";
            instruction_to_erase.push_back(&I);
          }
        }
      }
    }

    // Print "callgraph"
    for (const auto &pair : id_calls) {
      errs() << pair.first << ": ";
      for (const auto &called_fcn : pair.second) {
        errs() << called_fcn->getName() << ", ";
      }
      errs() << "\n";
    }

    for (auto &I : instruction_to_erase) {
      I->eraseFromParent();
    }
    return false;
  }
};
}

char TestPass::ID = 0;
static RegisterPass<TestPass> X("testpass", "Just a test pass.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);

/*
char SkeletonPass::ID = 0;

// Automatically enable the pass.
// http://adriansampson.net/blog/clangpass.html
static void registerSkeletonPass(const PassManagerBuilder &,
                         legacy::PassManagerBase &PM) {
  PM.add(new SkeletonPass());
}
static RegisterStandardPasses
  RegisterMyPass(PassManagerBuilder::EP_EarlyAsPossible,
                 registerSkeletonPass);
*/
