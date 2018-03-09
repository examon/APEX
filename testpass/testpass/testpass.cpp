#include <map>
#include <vector>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

using namespace llvm;

namespace {
struct TestPass : public ModulePass {
  static char ID;
  TestPass() : ModulePass(ID) {}

  std::map<StringRef, std::vector<Function *>> id_calls;
  std::vector<Instruction *> instruction_to_erase;

  virtual bool runOnModule(Module &M) {
    for (auto &F : M) {
      // This should be fine for global lookup.
      StringRef id = F.getGlobalIdentifier();

      id_calls[id] = std::vector<Function *>();
      for (auto &B : F) {
        for (auto &I : B) {
          // Check if we are on the call instruction. If yes, get it.
          if (auto callinst = dyn_cast<CallInst>(&I)) {
            Function *called_fcn = callinst->getCalledFunction();
            id_calls[id].push_back(called_fcn);

            if (called_fcn->getName() == "x") {
              // Collect desired instructions to be removed.
              instruction_to_erase.push_back(&I);
            }
          }
        }
      }
    }

    // We cannot remove instructions inside for loop, need to do it here.
    erase_collected_instructions();

    // print_callgraph();
    return false;
  }

  void erase_collected_instructions(void) {
    for (auto &I : instruction_to_erase) {
      I->eraseFromParent();
    }
  }

  void print_callgraph(void) {
    // Print "callgraph"
    for (const auto &pair : id_calls) {
      errs() << pair.first << ": ";
      for (const auto &called_fcn : pair.second) {
        errs() << called_fcn->getName() << ", ";
      }
      errs() << "\n";
    }
  }
};
}

char TestPass::ID = 0;
static RegisterPass<TestPass> X("testpass", "Just a test pass.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);
