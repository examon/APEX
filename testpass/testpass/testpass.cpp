#include <map>
#include <vector>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
using namespace llvm;

namespace {
  struct TestPass : public FunctionPass {
    static char ID;
    TestPass() : FunctionPass(ID) {}

    std::map<StringRef, std::vector<Function *>> id_calls;

    virtual bool runOnFunction(Function &F) {
      F.dump();

      // This should be fine for global lookup.
      StringRef id = F.getGlobalIdentifier();

      id_calls[id] = std::vector<Function *>();

      for (auto &B: F) {
        //B.dump();
        for (auto &I: B) {
          //I.dump();
          // Check if we are on the call instruction. If yes, get it.
          if (auto callinst = dyn_cast<CallInst>(&I)) {
            Function *called_fcn = callinst->getCalledFunction();
            id_calls[id].push_back(called_fcn);
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

      errs() << "\n=======\n";
      return false;
    }

    virtual bool doFinalization(Module &M) {
      errs() << "OUT\n";

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
