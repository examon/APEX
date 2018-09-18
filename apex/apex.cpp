// Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
//
// Published under Apache 2.0 license.
// See LICENSE for details.

// What is this:
// This file implements APEX as LLVM Pass (APEXPass). See build_and_run.sh for
// information about how to run APEXPass.

#include "apex.h"

/// Running on each module.
bool APEXPass::runOnModule(Module &M) {
  logDumpModule(M);

  APEXDependencyGraph apex_dg;
  LLVMDependenceGraph dg;

  // Initialize dg. This runs analysis and computes dependencies.
  dgInit(M, dg);

  // Initialize apex_dg, so we have dependencies in neat graph.
  apexDgInit(apex_dg);

  // Pretty print apex_dg.
  apexDgPrint(apex_dg, true);
  apexDgPrintDataDependeniesCompact(apex_dg);

  // Make graph out of data dependencies.
  apexDgMakeGraph(apex_dg);
  apexDgPrintGraph(apex_dg);

  // This is user input.
  // TODO: move this out
  std::string _SOURCE = "main";
  std::string _TARGET = "x";

  // Create call graph from module.
  std::vector<std::pair<Function *, std::vector<Function *>>> callgraph;
  createCallGraph(M, _SOURCE, callgraph);
  printCallGraph(callgraph);

  // Find path from @source to @target in the @callgraph.
  std::vector<Function *> path;
  findPath(callgraph, _SOURCE, _TARGET, path);
  printPath(path, _SOURCE, _TARGET);

  // TODO: THIS IS WIP
  logPrintUnderline("Calculating what functions to add into @path");
  functionVectorFlatPrint(path);
  Instruction *x_call_inst = nullptr;
  {
    for (auto &node_dependencies : apex_dg.node_data_dependencies_map) {
      Value *node_val = node_dependencies.first->getValue();
      if (isa<CallInst>(node_val)) {
        CallInst *call_inst = cast<CallInst>(node_val);
        Function *called_fcn = call_inst->getCalledFunction();
        std::string called_fcn_name = called_fcn->getName();
        logPrintFlat("IS CALL INST TO: " + called_fcn_name + ":");
        if (called_fcn_name == "x") {
          x_call_inst = call_inst;
        }
      }
      node_val->dump();
    }
  }
  {
    logPrintUnderline("TESTING");
    x_call_inst->dump();
    Function *source_fcn = M.getFunction(_SOURCE);
    source_fcn->dump();
    logPrint("I print");
    for (auto &BB : *source_fcn) {
      for (auto &I : BB) {
        if (isa<CallInst>(I)) {
          CallInst *call_inst = cast<CallInst>(&I);
          Function *called_fcn = call_inst->getCalledFunction();
          std::string called_fcn_name = called_fcn->getName();
          if (called_fcn_name == "x") {
            logPrint("x_call_inst");
            x_call_inst->dump();
            logPrint("I");
            I.dump();
            if (x_call_inst == &I) {
              logPrint("they are the same!");
            } else {
              logPrint("they are not the same?");
            }
          }
        }
      }
    }
  }

  // TODO: this is probably outdated and will need modification
  /*
  logPrintUnderline("Adding functions to @path.");
  std::vector<Function *> functions_to_process = path;
  while (false == functions_to_process.empty()) {
    logPrintFlat("current path: ");
    functionVectorFlatPrint(path);
    logPrintFlat("functions to process: ");
    functionVectorFlatPrint(functions_to_process);

    const Function *current = functions_to_process.back();
    functions_to_process.pop_back();
    logPrint("- currently examining: " + current->getGlobalIdentifier());

    std::vector<Function *> current_callees;
    functionGetCallees(current, current_callees);

    for (auto &callee : current_callees) {
      bool calee_to_be_processed = true;
      for (auto &path_fcn : path) {
        if (callee->getGlobalIdentifier() == path_fcn->getGlobalIdentifier()) {
          calee_to_be_processed = false;
        }
      }
      if (true == calee_to_be_processed) {
        functions_to_process.push_back(callee);
        path.push_back(callee);
        logPrint("  - adding: " + callee->getGlobalIdentifier());
      }
    }
    logPrint("");
  }
  */

  // Remove functions that do not affect calculated execution @path.
  logPrintUnderline("Removing functions that do not affect @path.");
  std::vector<Function *> functions_to_be_removed;
  { // Collect functions from module that will be removed
    // (functions that are not in the @path vector).
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
      logPrint(fcn->getName());
      if (functionRemove(fcn) < 0) {
        logPrint("[ERROR] Aborting. Was about to remove non-void returning "
                 "function!");
        return true;
      }
    }
  }

  return true;
}

// Logging utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Simple logging print with newline.
void APEXPass::logPrint(const std::string &message) {
  errs() << message + "\n";
}

/// Simple logging print with newline.
void APEXPass::logPrintUnderline(const std::string &message) {
  std::string underline = "";
  for (int i = 0; i < message.size() + 4; ++i) {
    underline.append("=");
  }
  errs() << "\n";
  errs() << underline + "\n";
  errs() << "# " + message + " #\n";
  errs() << underline + "\n";
}

/// Simple logging print WITHOUT newline.
void APEXPass::logPrintFlat(const std::string &message) { errs() << message; }

/// Dumps whole module M.
void APEXPass::logDumpModule(const Module &M) {
  std::string module_name = M.getModuleIdentifier();
  logPrintUnderline("module: " + module_name);
  M.dump();
  logPrint("");
}

// Function utilities ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Prints contents of the vector functions.
void APEXPass::functionVectorFlatPrint(
    const std::vector<Function *> &functions) {
  for (const Function *function : functions) {
    logPrintFlat(function->getGlobalIdentifier() + ", ");
  }
  logPrint("");
}

/// Takes pointer to function F, iterates over instructions calling this
/// function
/// and removes these instructions.
///
/// Returns: number of removed instructions, -1 in case of error.
int APEXPass::functionRemoveCalls(const Function *F) {
  int calls_removed = 0;

  if (nullptr == F) {
    return -1;
  }

  logPrint("- removing function calls to: " + F->getGlobalIdentifier());
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

/// Takes pointer to a function F, removes all calls to this function and then
/// removes function F itself.
///
/// Returns: 0 if successful or -1 in case of error.
int APEXPass::functionRemove(Function *F) {
  if (nullptr == F) {
    return -1;
  }
  std::string fcn_id = F->getGlobalIdentifier();

  logPrint("======= [to remove function: " + fcn_id + "] =======");

  if (functionRemoveCalls(F) < 0) {
    return -1;
  }

  std::string message = "removing function: ";
  message += F->getGlobalIdentifier();
  logPrint(message);

  F->eraseFromParent();

  logPrint("======= [to remove function: " + fcn_id + "] =======\n");
  return 0;
}

/// Collects functions that call function F into vector callers.
///
/// Returns: 0 in case of success, -1 if error.
int APEXPass::functionGetCallers(const Function *F,
                                 std::vector<Function *> &callers) {
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

/// Collects functions that are called by function F into vector callees.
///
/// Returns: 0 in case of success, -1 if error.
int APEXPass::functionGetCallees(const Function *F,
                                 std::vector<Function *> &callees) {
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

// Callgraph utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Creates callgraph from module M, starting from the function with global id
/// specified in root.
///
/// Callgraph is saved in vector callgraph in the pairs of the following format:
/// <caller function, vector of called functions>.
///
/// Returns: 0 in case of success, -1 if error.
int APEXPass::createCallGraph(
    const Module &M, const std::string &root,
    std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph) {
  logPrintUnderline("createCallGraph(): Building call graph");
  Function *root_fcn = M.getFunction(root);
  if (nullptr == root_fcn) {
    logPrint("- root function should not be nullptr after collecting [ERROR]");
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
      if (functionGetCallees(node, node_callees) < 0) {
        logPrint("- failed to collect target callees [ERROR]");
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
  logPrint("- done");
  return 0;
}

/// Prints callgraph.
void APEXPass::printCallGraph(
    const std::vector<std::pair<Function *, std::vector<Function *>>>
        &callgraph) {
  logPrintUnderline("Printing callgraph");
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
    logPrint(caller + callees);
  }
}

/// Uses BFS to find path in callgraph from source to target
/// (both are global Function IDs).
///
/// Result is stored in the final_path vector.
///
/// Returns: 0 in success, -1 if error.
int APEXPass::findPath(
    const std::vector<std::pair<Function *, std::vector<Function *>>>
        &callgraph,
    const std::string &source, const std::string &target,
    std::vector<Function *> &final_path) {

  logPrintUnderline("findPath(): Finding path from @" + source + " to @" +
                    target);
  std::vector<std::vector<std::string>> queue;
  std::vector<std::string> v_start;
  v_start.push_back(source);
  queue.push_back(v_start);

  while (false == queue.empty()) {
    std::vector<std::string> path = queue.back();
    queue.pop_back();

    std::string node = path.back();

    // Found the end. Store function pointers to @final_path.
    if (node == target) {
      for (const std::string node_id : path) {
        for (auto &caller_callees : callgraph) {
          Function *caller = caller_callees.first;
          if (caller->getGlobalIdentifier() == node_id) {
            final_path.push_back(caller);
          }
        }
      }
      logPrint("- done");
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

  logPrint("- Unable to find path from @" + source + " to @" + target +
           " [ERROR]");

  return -1;
}

/// Prints path.
void APEXPass::printPath(const std::vector<Function *> &path,
                         const std::string &source, const std::string &target) {
  logPrintUnderline("printPath(): " + source + " -> " + target);
  for (auto &node : path) {
    logPrint(node->getGlobalIdentifier());
  }
}

// dg utilities +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Initializes dg and calculates control & data dependencies.
void APEXPass::dgInit(Module &M, LLVMDependenceGraph &dg) {
  if (_LOG_VERBOSE) {
    logPrintUnderline("dgInit(): Building dependence graph");
  }
  // The reasoning why everything below needs to be here:
  //
  // In order to get data dependencies, I've replicated
  // what llvm-dg-dump tool is doing, so for details, check:
  // dg/tools/llvm-dg-dump.cpp
  LLVMPointerAnalysis *pta = new LLVMPointerAnalysis(&M);
  pta->run<analysis::pta::PointsToFlowInsensitive>();
  dg.build(&M, pta);
  analysis::rd::LLVMReachingDefinitions rda(&M, pta);
  rda.run<analysis::rd::ReachingDefinitionsAnalysis>();
  // rda.run<analysis::rd::SemisparseRda>(); // This is alternative to ^
  LLVMDefUseAnalysis dua(&dg, &rda, pta);
  dua.run();
  dg.computeControlDependencies(CD_ALG::CLASSIC);
  if (_LOG_VERBOSE) {
    logPrint("- done");
  }
}

// apex dg utilities ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Extracts data from dg and stores them into apex_dg structure.
///
/// Caution: dgInit() has to be called before apexDgInit(), dg needs to be
///          initialized in for this to properly work -> CF map will be empty
void APEXPass::apexDgInit(APEXDependencyGraph &apex_dg) {
  logPrintUnderline("apexDgInit(): Building apex dependency graph");
  const std::map<llvm::Value *, LLVMDependenceGraph *> &CF =
      getConstructedFunctions(); // Need to call this (dg reasons...).

  for (auto &function_dg : CF) {
    APEXDependencyFunction apex_function;
    apex_function.value = function_dg.first;

    for (auto &value_block : function_dg.second->getBlocks()) {
      // We do not care about blocks, we care about function:node/istruction
      // relationship.
      for (auto &node : value_block.second->getNodes()) {
        // Create node/instruction structure and fill it with control/data
        // dependencies that is has to other nodes.
        APEXDependencyNode apex_node;
        apex_node.node = node;
        apex_node.value = node->getValue();

        // TODO: Need to figure out if any function from @path has dependencies
        //       and if those dependencies contain other function calls. If they
        //       do, add those called functions into @path and recursively
        //       repeat.
        if (isa<CallInst>(apex_node.value)) {
          logPrint("is a call inst");
        } else {
          logPrint("is not call inst");
        }
        /*
        if (auto callinst = dyn_cast<CallInst>(&apex_node.value)) {
          Function *called_fcn = callinst->getCalledFunction();
          if (nullptr == called_fcn) {
            logPrint("called_fcn is nullptr");
          } else {
            std::string fcn_name = called_fcn->getName();
            logPrint("fcn: " + fcn_name);
          }
        }
         */

        apexDgGetBlockNodeInfo(apex_node, node);
        apex_function.nodes.push_back(apex_node);
      }
    }
    apex_dg.functions.push_back(apex_function);
  }
  logPrint("- done");
}

/// Stores control/reverse_control/data/reverse_data dependencies of the node
/// to the apex_node.
void APEXPass::apexDgGetBlockNodeInfo(APEXDependencyNode &apex_node,
                                      LLVMNode *node) {
  for (auto i = node->control_begin(), e = node->control_end(); i != e; ++i) {
    LLVMNode *cd_node = *i;
    apex_node.control_depenencies.push_back(cd_node);
  }

  for (auto i = node->rev_control_begin(), e = node->rev_control_end(); i != e;
       ++i) {
    LLVMNode *rev_cd_node = *i;
    apex_node.rev_control_depenencies.push_back(rev_cd_node);
  }

  for (auto i = node->data_begin(), e = node->data_end(); i != e; ++i) {
    LLVMNode *dd_node = *i;
    apex_node.data_dependencies.push_back(dd_node);
  }

  for (auto i = node->rev_data_begin(), e = node->rev_data_end(); i != e; ++i) {
    LLVMNode *rev_dd_node = *i;
    apex_node.rev_data_dependencies.push_back(rev_dd_node);
  }
}

/// Pretty prints APEXDependencyGraph structure.
void APEXPass::apexDgPrint(APEXDependencyGraph &apex_dg, bool only_data) {
  logPrintUnderline("APEXDependencyGraph");
  logPrint("== [functions dump]:");

  for (APEXDependencyFunction &function : apex_dg.functions) {
    // Print function info.
    std::string function_name = function.value->getName();
    logPrintFlat("   == function [" + function_name + "] value address: ");
    errs() << function.value << "\n";

    // Print node info.
    logPrint("\n      = [nodes dump]:");
    for (APEXDependencyNode &node : function.nodes) {
      // Dump node value address.
      logPrintFlat("        = node value address: ");
      errs() << node.value;
      logPrint("");

      // Dump node values, so we can see instruction itself and not only
      // address.
      logPrintFlat("        = node value: ");
      node.value->dump();
      logPrint("");

      // Print dependencies.
      if (false == only_data) {
        logPrint("          - [cd]:");
        for (LLVMNode *cd : node.control_depenencies) {
          logPrintFlat("             - ");
          std::string cd_name = cd->getValue()->getName();
          errs() << cd->getValue() << " [" << cd_name << "]\n";
        }
      }

      if (false == only_data) {
        logPrint("          - [rev cd]:");
        for (LLVMNode *rev_cd : node.rev_control_depenencies) {
          logPrintFlat("            - ");
          std::string rev_cd_name = rev_cd->getValue()->getName();
          errs() << rev_cd->getValue() << " [" << rev_cd_name << "]\n";
        }
      }

      logPrint("          - [dd]:");
      for (LLVMNode *dd : node.data_dependencies) {
        logPrintFlat("            - ");
        std::string dd_name = dd->getValue()->getName();
        errs() << dd->getValue() << " [" << dd_name << "]\n";

        if (only_data && dd_name.size() == 0) {
          dd->getValue()->dump();
        }
      }

      logPrint("          - [rev dd]:");
      for (LLVMNode *rev_dd : node.rev_data_dependencies) {
        logPrintFlat("            - ");
        std::string rev_dd_name = rev_dd->getValue()->getName();
        errs() << rev_dd->getValue() << " [" << rev_dd_name << "]\n";

        if (only_data && rev_dd_name.size() == 0) {
          rev_dd->getValue()->dump();
        }
      }

      logPrint("");
    }
  }
}

/// Pretty prints apex_dg, but only with data dependencies.
void APEXPass::apexDgPrintDataDependeniesCompact(APEXDependencyGraph &apex_dg) {
  logPrintUnderline("APEXDependencyGraph : data dependencies");
  for (APEXDependencyFunction &function : apex_dg.functions) {
    std::string fcn_name = function.value->getName();
    logPrint("\n=> " + fcn_name);
    for (APEXDependencyNode &node : function.nodes) {

      logPrintFlat("\n===> node:");
      node.value->dump();
      logPrint("[dd]:");

      for (LLVMNode *dd : node.data_dependencies) {
        dd->getValue()->dump();
      }

      logPrint("[rev_dd]:");
      for (LLVMNode *rev_dd : node.rev_data_dependencies) {
        rev_dd->getValue()->dump();
      }
    }
  }
}

/// Takes apex_dg, makes graph out of data dependencies between nodes
/// and stores this graph into apex_dg.graph variable.
void APEXPass::apexDgMakeGraph(APEXDependencyGraph &apex_dg) {
  logPrintUnderline("apexDgMakeGraph(): building data dependencies graph");

  for (APEXDependencyFunction &f : apex_dg.functions) {
    for (APEXDependencyNode &n : f.nodes) {
      apex_dg.node_data_dependencies_map[n.node] = n.data_dependencies;
      apex_dg.node_function_map[n.node] = &f;
    }
  }
  logPrint(" - done");
}

/// Prints node_data_dependencies_map from the apex_dg structure.
void APEXPass::apexDgPrintGraph(APEXDependencyGraph &apex_dg) {
  logPrintUnderline("apexDgPrintGraph(): ");
  for (auto &node_dependencies : apex_dg.node_data_dependencies_map) {
    errs() << "KEY: " << node_dependencies.first << "\n";
    node_dependencies.first->getValue()->dump();
    StringRef fcn_name =
        apex_dg.node_function_map[node_dependencies.first]->value->getName();
    errs() << "IN FCN: " << fcn_name << "\n";
    logPrint("");
    for (LLVMNode *dd : node_dependencies.second) {
      errs() << "    *** " << dd << "\n";
      dd->getValue()->dump();
      logPrint("");
    }
    logPrint("===");
  }
}