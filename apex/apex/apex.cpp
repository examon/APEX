/* Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
 *
 * Published under Apache 2.0 license.
 * See LICENSE for details.
 */

// What is this:
// This file implements APEXPass, that is llvm pass that can be run via opt.




#include <string>
#include <vector>

#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

#include "apex_config.h"

using namespace llvm;

namespace {
struct APEXPass : public ModulePass {
  static char ID;
  APEXPass() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;
};
}

/* Registering our own pass, so it can be ran via opt.
 */
char APEXPass::ID = 0;
static RegisterPass<APEXPass> X("apex", "Just a test pass. Work in progress.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);

// Debug utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void debugDumpModule(const Module &M) {
  if (_DEBUG) {
    errs() << ">>>>>>>>> Module dump() BEGINS:\n\n";
    M.dump();
    errs() << "<<<<<<<<< Module dump() ENDS!\n";
  }
}

void debugPrint(const std::string &message) {
  if (_DEBUG) {
    errs() << ">>>>>>>>> debugPrint(): " + message + "\n";
  }
}

// Log utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void logPrint(const std::string &message) {
  if (_LOG) {
    errs() << "+++++++++ logPrint(): " + message + "\n";
  }
}

// Function utilities ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Takes pointer to function, iterates over instruction calling this function
// and removes these instructions.
//
// Returns: number of removed instructions, -1 in case of error
int removeFunctionCalls(const Function *F) {
  int calls_removed = 0;

  if (nullptr == F) {
    return -1;
  }

  logPrint("Proceeding to remove call instructions to: " +
           F->getGlobalIdentifier());
  for (const Use &use : F->uses()) {
    if (Instruction *UserInst = dyn_cast<Instruction>(use.getUser())) {
      std::string message = "- in: ";
      message += UserInst->getFunction()->getName();
      message += "; removing call instruction to: ";
      message += F->getName();

      if (!UserInst->getType()->isVoidTy()) {
        // NOTE: Currently only removing void returning calls.
        // TODO: handle non-void returning function calls in the future
        message += "; removing non-void returning call is not yet supported! ";
        message += "[ABORT]";
        logPrint(message);
        return -1;
      }

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

  logPrint("Proceeding to remove function: " + F->getGlobalIdentifier());

  if (removeFunctionCalls(F) < 0) {
    return -1;
  }

  std::string message = "- removing function: ";
  message += F->getGlobalIdentifier();
  logPrint(message);

  F->eraseFromParent();
  return 0;
}

// TODO: Not used ATM.
// Collects functions that call function @F into vector @callers.
//
// Returns: 0 in case of success, -1 if error.
int getFunctionCallers(const Function *F, std::vector<Function *> &callers) {
  if (nullptr == F) {
    return -1;
  }

  for (const Use &use : F->uses()) {
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
int getFunctionCallees(const Function *F, std::vector<Function *> &callees) {
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
// specified in @root. Callgraph is saved in vector @callgraph in the pairs
// of the following format: <caller function, vector of called functions>.
//
// Returns: 0 in case of success, -1 if error.
int createCallGraph(
    const Module &M, const std::string &root,
    std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph) {
  // Collect root function.
  Function *root_fcn = M.getFunction(root);
  if (nullptr == root_fcn) {
    logPrint("root function should not be nullptr after collecting [ERROR]");
    return -1;
  }

  { // Construct call graph.
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
    const std::vector<std::pair<Function *, std::vector<Function *>>>
        &callgraph) {
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
// Function IDs). Result is stored in the @final_path.
//
// Returns: 0 in success, -1 if error.
int findPath(const std::vector<std::pair<Function *, std::vector<Function *>>>
                 &callgraph,
             const std::string &start, const std::string &end,
             std::vector<Function *> &final_path) {
  { // Computes path, stores
    std::vector<std::vector<std::string>> queue;
    std::vector<std::string> v_start;
    v_start.push_back(start);
    queue.push_back(v_start);

    while (false == queue.empty()) {
      std::vector<std::string> path = queue.back();
      queue.pop_back();

      std::string node = path.back();

      // Found the end. Store function pointers to @final_path.
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

      // Find adjacent/called nodes of @node and save them to @callees.
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
bool APEXPass::runOnModule(Module &M) {
  debugDumpModule(M);

  // Create callgraph from module.
  std::vector<std::pair<Function *, std::vector<Function *>>> callgraph;
  createCallGraph(M, _SOURCE, callgraph);
  printCallGraph(callgraph);

  // Find path from @source to @target in the @callgraph.
  std::vector<Function *> path;
  findPath(callgraph, _SOURCE, _TARGET, path);
  for (auto &node : path) {
    debugPrint(node->getGlobalIdentifier());
  }

  // Remove functions that do not affect calculated execution @path.
  std::vector<Function *> functions_to_be_removed;
  { // Collect functions from module that will be removed.
    for (auto &module_fcn : M.getFunctionList()) {
      bool module_fcn_to_be_removed = true;
      for (Function *path_fcn : path) {
        if (module_fcn.getGlobalIdentifier() ==
            path_fcn->getGlobalIdentifier()) {
          module_fcn_to_be_removed = false;
        }
      }
      if (true == module_fcn_to_be_removed) {
        functions_to_be_removed.push_back(&module_fcn);
      }
    }
  }
  { // Remove collected functions.
    for (auto &fcn : functions_to_be_removed) {
      debugPrint(fcn->getGlobalIdentifier());
      removeFunction(fcn);
    }
  }

  debugDumpModule(M);
  return true;
}