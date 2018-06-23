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

void debugPrint(std::string message) {
  if (_DEBUG) {
    errs() << ">>>>>>>>> debugPrint(): " + message + "\n";
  }
}

// Log utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void logPrint(std::string message) {
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
        // NOTE: Currently only removing void returning calls.
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

// Takes pointer to a function, removes all calls to this function and then
// removes function itself.
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

// Creates callgraph from module @M, starting from the function with global id
// specified in @root. Callgraph is saved in vector @callgraph in the pair
// of the following format: <caller function, vector of called functions>.
//
// Returns: 0 in case of success, -1 if error.
int createCallGraph(
    Module &M, std::string root,
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

    while (false == queue.empty()) {
      Function *node = queue.back();
      queue.pop_back();

      // Find call functions that are called by the node.
      std::vector<Function *> node_callees;
      if (getFunctionCallees(node, node_callees) < 0) {
        logPrint("failed to collect target callees [ERROR]");
        return -1;
      }

      // Make caller->callees pair and save it.
      std::pair<Function *, std::vector<Function *>> caller_callees;
      caller_callees.first = node;
      caller_callees.second = node_callees;
      callgraph.push_back(caller_callees);

      // Put callees to queue, they will be examined next.
      for (Function *callee : node_callees) {
        queue.push_back(callee);
      }
    }
  }
  return 0;
}

// Prints @callgraph.
void printCallGraph(
    std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph) {
  for (auto &caller_callees : callgraph) {
    std::string caller = caller_callees.first->getGlobalIdentifier();
    std::string callees = " -> ";
    if (caller_callees.second.empty()) {
      callees += "[External/Nothing]";
    }
    for (auto &callee : caller_callees.second) {
      callees += callee->getGlobalIdentifier();
      callees += ", ";
    }
    logPrint("callgraph: " + caller + callees);
  }
}

// Uses BFS to find path in @callgraph from @start to @end (both are global
// IDs). Result is stored in the @final_path.
//
// Returns: 0 in success, -1 if error.
int findPath(
    std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph,
    std::string start, std::string end, std::vector<Function *> &final_path) {
  {  // Computes path, stores
    std::vector<std::vector<std::string>> queue;
    std::vector<std::string> v_start;
    v_start.push_back(start);
    queue.push_back(v_start);

    while (false == queue.empty()) {
      std::vector<std::string> path = queue.back();
      queue.pop_back();

      std::string node = path.back();

      // Fund the end. Store function pointers to @final_path.
      if (node == end) {
        for (const std::string node_id : path) {
          for (auto &caller_callees : callgraph) {
            Function *caller = caller_callees.first;
            if (caller->getGlobalIdentifier() == node_id) {
              final_path.push_back(caller);
            }
          }
        }
        return 0;
      }

      // Find adjacent/called nodes to @node and save them to @callees.
      std::vector<Function *> callees;
      for (auto &caller_callees : callgraph) {
        if (node == caller_callees.first->getGlobalIdentifier()) {
          callees = caller_callees.second;
        }
      }

      // Iterate over adjacent/called nodes and add them to the path.
      for (Function *callee : callees) {
        std::string callee_id = callee->getGlobalIdentifier();
        std::vector<std::string> new_path = path;
        new_path.push_back(callee_id);
        queue.push_back(new_path);
      }
    }
  }

  return -1;
}

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Running on each module.
bool TestPass::runOnModule(Module &M) {
  debugDumpModule(M);

  std::vector<std::pair<Function *, std::vector<Function *>>> callgraph;
  createCallGraph(M, "main", callgraph);
  printCallGraph(callgraph);

  std::vector<Function *> path;
  findPath(callgraph, "main", "y", path);
  for (auto &node : path) {
    debugPrint(node->getGlobalIdentifier());
  }

  // TODO: remove every function not present in the @path
  //  debugDumpModule(M);
  return true;
}