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

// Discussion about call graphs
// https://groups.google.com/forum/#!topic/llvm-dev/peP9L8rjFvA

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

  /* Our own stuff.
   */
  int traverse_callgraph(Module &module, StringRef source, StringRef target,
                         std::vector<Function *> &functions_to_remove);
};
}

/* Registering our own pass, so it can be ran via opt.
 */
char TestPass::ID = 0;
static RegisterPass<TestPass> X("testpass",
                                "Just a test pass. Work in progress.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);

// Debug utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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

// Log utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void logPrint(StringRef message) {
  if (_LOG) {
    errs() << "+++++++++ logPrint(): " + message + "\n";
  }
}

// Function utilities ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Takes pointer to function, iterates over instruction calling this function
// and removes these instructions.
//
// Returns: number of removed instructions, -1 in case of error
int removeFunctionCalls(Function *F) {
  int calls_removed = 0;

  if (nullptr == F) {
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
// Returns: 0 if successful or -1 in case of error.
int removeFunction(Function *F) {
  if (nullptr == F) {
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

// Collects functions that call function @F into vector @callers.
//
// Returns: 0 in case of success, -1 if error.
int getFunctionCallers(Function *F, std::vector<Function *> &callers) {
  if (nullptr == F) {
    return -1;
  }

  for (Use &use : F->uses()) {
    if (Instruction *UserInst = dyn_cast<Instruction>(use.getUser())) {
      if (!UserInst->getType()->isVoidTy()) {
        // Currently only removing void returning calls.
        // TODO: handle non-void returning function calls in the future
        return -1;
      }
      callers.push_back(UserInst->getFunction());
    } else {
      return -1;
    }
  }
  return 0;
}

// Collects functions that are called by function @F into vector @callees.
//
// Returns: 0 in case of success, -1 if error.
int getFunctionCallees(Function *F, std::vector<Function *> &callees) {
  if (nullptr == F) {
    return -1;
  }

  for (auto &BB : *F) {
    for (auto &I : BB) {
      if (auto callinst = dyn_cast<CallInst>(&I)) {
        Function *called_fcn = callinst->getCalledFunction();
        if (nullptr == called_fcn) {
          logPrint("called_fcn is nullptr [ERROR]");
          return -1;
        }
        callees.push_back(called_fcn);
      }
    }
  }
  return 0;
}

// Graph utilities ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
int createCallGraph(
    Module &M, StringRef root,
    std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph) {
  // Collect root function.
  Function *root_fcn = M.getFunction(root);
  if (nullptr == root_fcn) {
    logPrint("root function should not be nullptr after collecting [ERROR]");
    return -1;
  }

  {  // Construct call graph.
    std::vector<Function *> queue;
    queue.push_back(root_fcn);

    while (!queue.empty()) {
      Function *node = queue.back();
      queue.pop_back();
      std::vector<Function *> target_callees;
      if (getFunctionCallees(node, target_callees) < 0) {
        logPrint("failed to collect target callees [ERROR]");
        return -1;
      }
      for (Function *callee : target_callees) {
        queue.push_back(callee);
        std::pair<Function *, std::vector<Function *>> caller_callees;
        caller_callees.first = node;
        caller_callees.second = target_callees;
        callgraph.push_back(caller_callees);
      }
    }
  }
  return 0;
}

void printCallGraph(
    std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph) {
  for (auto &caller_callees : callgraph) {
    std::string caller = caller_callees.first->getGlobalIdentifier();
    std::string callees = " -> ";
    for (auto &callee : caller_callees.second) {
      callees += callee->getGlobalIdentifier();
      callees += ", ";
    }
    logPrint("callgraph: " + caller + callees);
  }
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Running on each module.
bool TestPass::runOnModule(Module &M) {
  debugDumpModule(M);
  std::vector<Function *> function_to_remove;
  std::vector<std::pair<Function *, std::vector<Function *>>> callgraph;
  createCallGraph(M, "main", callgraph);
  printCallGraph(callgraph);

  //  debugDumpModule(M);
  return true;
}

int TestPass::traverse_callgraph(Module &module, StringRef source,
                                 StringRef target,
                                 std::vector<Function *> &functions_to_remove) {
  CallGraph CallGraph(module);  // This is better than getAnalysis
  debugDumpCallGraph(CallGraph);

  CallGraphNode *source_node = nullptr;
  CallGraphNode *target_node = nullptr;

  {  // Collect source and target nodes from callgraph.
    for (auto &node : CallGraph) {
      CallGraphNode *graph_node = node.second.get();

      /* NOTE:
         I don't know if CallGraph node can be null, but putting this in here
         just in case.
       */
      if (nullptr == graph_node) {
        debugPrint("graph_node is NULL");
        continue;
      }

      /* Indirect calls can cause function within the call graph node to be
         null. Need to check for this or there will be segfault.

         This is going to fail:

            auto fcn = graph_node->getFunction();
            auto fcn = node.first();

         Need to first check:

            node.first == nullptr
       */
      const Function *const fcn_const = node.first;
      if (nullptr == fcn_const) {
        debugPrint("fnc is NULL");
        continue;
      }

      if (fcn_const->getGlobalIdentifier() == source) {
        source_node = graph_node;
      } else if (fcn_const->getGlobalIdentifier() == target) {
        target_node = graph_node;
      }
    }
  }

  {  // Functions outside of execution path will be removed.
    if (nullptr == source_node) {
      logPrint("source node should not be nullptr after collecting [ERROR]");
      return -1;
    }

    if (nullptr == target_node) {
      logPrint("target node should not be nullptr after collecting [ERROR]");
      return -1;
    }

    Function *target_fcn = target_node->getFunction();
    for (Use &use : target_fcn->uses()) {
      //      if (Instruction *UserInst = dyn_cast<Instruction>(use.getUser()))
      //      {
      //        std::string message = "in: ";
      //        // message += UserInst->getFunction()->getName();
      //      }

      //        if (!UserInst->getType()->isVoidTy()) {
      //          // Currently only removing void returning calls.
      //          // TODO: handle non-void returning function calls in the
      //          future
      //          message += ", removing non-void returning call is not a good
      //          idea! ";
      //          message += "[ERROR]";
      //          logPrint(message);
      //          return -1;
      //        }
      //        message += ", removing call instruction to: ";
      //        message += target_fcn->getName();
      //        logPrint(message);
      //      } else {
      //        return -1;
      //      }
    }
  }

  //    for (auto i = CallGraph.begin(); i != CallGraph.end(); ++i) {
  //      const Function *const f = i->first;
  //      if (nullptr == f) {
  //        continue;
  //      }
  //      f->dump();
  //    }

  //    for (auto &f : *target_node) {
  //      if (nullptr == f.second) {
  //        continue;
  //      }
  // CallGraphNode *node = f.second;
  //  }
}