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
  std::vector<std::pair<Function *, std::vector<Function *>>> call_graph;
  std::vector<Function *> path;

  // Initialize dg. This runs analysis and computes dependencies.
  dgInit(M, dg);

  // Initialize apex_dg, so we have dependencies stored in neat little graph.
  apexDgInit(apex_dg);
  //  apexDgPrint(apex_dg, true);
  //  apexDgPrintDataDependeniesCompact(apex_dg);

  // Use @apex_dg and make graph only out of data dependencies.
  // Store everything back into the @apex_dg.
  apexDgMakeGraph(apex_dg);
  //  apexDgPrintGraph(apex_dg);

  // Create call graph from module.
  createCallGraph(M, ARG_SOURCE_FCN, call_graph);
  //  printCallGraph(call_graph);

  // Find path from @source to @target in the @callgraph. Save it to @path.
  findPath(call_graph, ARG_SOURCE_FCN, ARG_TARGET_FCN, path);
  printPath(path, ARG_SOURCE_FCN, ARG_TARGET_FCN);

  // Resolve data dependencies for functions in @path.
  updatePathAddDependencies(path, apex_dg);

  // Remove all functions (along with dependencies) that are not in the @path.
  moduleRemoveFunctionsNotInPath(M, apex_dg, path);

  // TODO: WIP
  // 1. Figure out at which call to ARG_TARGET_FCN we want to stop and replace
  //    that call with return.

  logPrintUnderline("Getting callsite for @" + ARG_TARGET_FCN);
  {
    Function *target_fcn = M.getFunction(ARG_TARGET_FCN);
    if (nullptr == target_fcn) {
      logPrint("ERROR: Could not get target function from module.");
      return true;
    }

    std::vector<Function *> target_fcn_callers;
    if (functionGetCallers(target_fcn, target_fcn_callers) != 0) {
      logPrint("ERROR: Could not get callers for target function.");
    }

    logPrint("Collected function callers:");
    for (Function *f : target_fcn_callers) {
      f->dump();
      logPrint("- " + f->getGlobalIdentifier());
    }

    logPrint("Picking one caller:");
    // Ok, so we have @target_fcn_callers AKA functions that call @target.
    // There can be multiple callers, so we need to decide which one to
    // pick.
    // We will pick one that is immediately before @target in @path, e.g.:
    // If, @path = [main, n, y], @target = y, so we will pick @n.
    for (int i = 0; i < path.size(); ++i) {
      logPrint(path[i]->getGlobalIdentifier());
    }
  }

  // 2. We will need to get all data dependencies from this call and remove them
  //    before replacing call with return, so that there are no segfaults????
  // 3. done?

  logPrintUnderline("APEXPass END");
  return true;
}

// Logging utilities
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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

// Function utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Prints contents of the vector functions.
void APEXPass::functionVectorFlatPrint(
    const std::vector<Function *> &functions) {
  for (const Function *function : functions) {
    logPrintFlat(function->getGlobalIdentifier() + ", ");
  }
  logPrint("");
}

/// Prints contents of the vector functions.
void APEXPass::functionListFlatPrint(const std::list<Function *> &functions) {
  for (const Function *function : functions) {
    logPrintFlat(function->getGlobalIdentifier() + ", ");
  }
  logPrint("");
}

/// Takes pointer to function F, iterates over instructions calling this
/// function and removes these instructions.
///
/// Returns: Number of removed instructions, -1 in case of error.
int APEXPass::functionRemoveCalls(APEXDependencyGraph &apex_dg,
                                  const Function *F) {
  int calls_removed = 0;

  if (nullptr == F) {
    return -1;
  }

  std::string fcn_name = F->getGlobalIdentifier();

  logPrint("- removing function calls to: " + F->getGlobalIdentifier());
  for (const Use &use : F->uses()) {
    if (Instruction *UserInst = dyn_cast<Instruction>(use.getUser())) {
      std::string message = "- in: ";
      message += UserInst->getFunction()->getName();
      message += "; removing call instruction to: ";
      message += F->getName();

      // TODO: I don't know why this needs to be here.
      // TODO: If it is outside, e.g. in the functionRemove(), it segfaults.
      // TODO: Needs refactoring. This is retarded.
      //      functionRemoveDependencies(apex_dg, fcn_name);

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
int APEXPass::functionRemove(APEXDependencyGraph &apex_dg, Function *F) {
  if (nullptr == F) {
    return -1;
  }
  std::string fcn_id = F->getGlobalIdentifier();

  logPrint("functionRemove(): " + fcn_id);

  // When we are calling this, all calls should be already deleted.
  if (functionRemoveCalls(apex_dg, F) < 0) {
    return -1;
  }

  std::string message = "removing function: ";
  message += fcn_id;
  logPrint(message);

  F->eraseFromParent();
  logPrint("- done\n");
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

/// Finds function calls to function with global id @function_name in
/// the @apex_dg, finds all its reverse and forward dependencies and stores
/// them into @dependencies vector.
void APEXPass::functionCollectDependencies(
    APEXDependencyGraph &apex_dg, std::string function_name,
    std::vector<LLVMNode *> &dependencies) {
  for (auto &node_fcn : apex_dg.node_function_map) {
    LLVMNode *node = node_fcn.first;
    Value *val = node->getValue();

    if (isa<CallInst>(val)) {
      CallInst *curr_call_inst = cast<CallInst>(val);
      Function *curr_fcn = curr_call_inst->getCalledFunction();
      std::string curr_fcn_name = curr_fcn->getName();

      if (curr_fcn_name == function_name) {
        std::vector<LLVMNode *> data_dependencies;
        std::vector<LLVMNode *> rev_data_dependencies;
        apexDgFindDataDependencies(apex_dg, *node, data_dependencies,
                                   rev_data_dependencies);
        // Append found dependencies
        dependencies.insert(dependencies.end(), data_dependencies.begin(),
                            data_dependencies.end());
      }
    }
  }
}

// Callgraph utilities
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Creates callgraph from module M, starting from the function with global id
/// specified in root.
///
/// Callgraph is saved in vector callgraph in the pairs of the following
/// format:
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

/// Uses BFS to find path in @callgraph from @source to @target
/// (both are global Function IDs).
///
/// Result is stored in the @final_path.
///
/// Returns: 0 in success, -1 if error.
// TODO: Implement proper BFS (with tagging visited nodes).
/*
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
*/

int APEXPass::findPath(
    const std::vector<std::pair<Function *, std::vector<Function *>>>
        &callgraph,
    const std::string &source, const std::string &target,
    std::vector<Function *> &final_path) {
  logPrintUnderline("findPath(): Finding path from @" + source + " to @" +
                    target);

  logPrint("Searching for @" + source + " function in callgraph:");
  const std::pair<Function *, std::vector<Function *>> *source_node = nullptr;
  {
    for (auto &node_neighbours : callgraph) {
      Function *node = node_neighbours.first;
      if (source == node->getGlobalIdentifier()) {
        source_node = &node_neighbours;
        break;
      }
    }
    if (nullptr == source_node) {
      logPrint("ERROR: Could not find source function.");
      return -1;
    } else {
      logPrint("- done");
    }
  }

  logPrint("\nRunning BFS to find path from @" + source + " to @" + target +
           ":");
  std::map<Function *, std::vector<Function *>> function_path;
  {
    std::vector<std::pair<Function *, std::vector<Function *>>> queue;
    std::vector<Function *> visited;

    queue.push_back(*source_node);

    while (false == queue.empty()) {
      std::pair<Function *, std::vector<Function *>> current = queue.back();
      queue.pop_back();
      Function *node = current.first;
      std::vector<Function *> neighbours = current.second;

      // Store current node as visited.
      visited.push_back(node);

      for (Function *neighbour : current.second) {
        // Store @main->@neighbour path.
        function_path[neighbour].push_back(current.first);

        // Check if we already visited @neighbour.
        bool neighbour_already_visited = false;
        for (Function *visited_fcn : visited) {
          if (visited_fcn == neighbour) {
            neighbour_already_visited = true;
            break;
          }
        }
        // Add @neighbour to the queue along with its neighbours if it was
        // not visited already.
        if (false == neighbour_already_visited) {
          for (auto &node_neighbours : callgraph) {
            if (node_neighbours.first == neighbour) {
              queue.push_back(node_neighbours);
            }
          }
        }
      }
    }
    logPrint("- done");
  }

  logPrint("\nReconstructing and storing @" + source + " -> @" + target +
           " path to the @final_path:");
  {
    for (auto &node_path : function_path) {
      // Need to pull @target first.
      if (node_path.first->getGlobalIdentifier() == target) {
        // We push path that was stored. First node should be @source.
        for (Function *node : node_path.second) {
          final_path.push_back(node);
        }
        // Finally, push @target node itself.
        final_path.push_back(node_path.first);
        logPrint("- done");
      }
    }
  }
}

/// Prints path.
void APEXPass::printPath(const std::vector<Function *> &path,
                         const std::string &source, const std::string &target) {
  logPrintUnderline("printPath(): " + source + " -> " + target);

  for (auto &node : path) {
    logPrint(node->getGlobalIdentifier());
  }
}

// dg utilities
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Initializes dg and calculates control & data dependencies.
void APEXPass::dgInit(Module &M, LLVMDependenceGraph &dg) {
  logPrintUnderline("dgInit(): Building dependence graph");

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
  // rda.run<analysis::rd::SemisparseRda>(); // This is alternative to above
  // ^^
  LLVMDefUseAnalysis dua(&dg, &rda, pta);
  dua.run();
  dg.computeControlDependencies(CD_ALG::CLASSIC);

  logPrint("- done");
}

// apex dg utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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
    logPrint("\n===> " + fcn_name);
    for (APEXDependencyNode &node : function.nodes) {

      logPrintFlat("\n=> node:");
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
      apex_dg.node_rev_data_dependencies_map[n.node] = n.rev_data_dependencies;
      apex_dg.node_function_map[n.node] = &f;
    }
  }
  logPrint("- done");
}

/// Prints node_data_dependencies_map from the apex_dg structure.
void APEXPass::apexDgPrintGraph(APEXDependencyGraph &apex_dg) {
  logPrintUnderline("apexDgPrintGraph():");

  for (auto &node_dependencies : apex_dg.node_rev_data_dependencies_map) {
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

// TODO: Refactor this function.
/// Amazing, we have instruction that is calling some function.
/// Now check if this instruction has dependencies.
///
/// If it has dependencies and any of those is function call
/// that is not in the @path, add function that it calls to
/// the path.
void APEXPass::apexDgNodeResolveDependencies(std::vector<Function *> &path,
                                             APEXDependencyGraph &apex_dg,
                                             const APEXDependencyNode &node) {
  for (auto n : apex_dg.node_data_dependencies_map) {
    if (n.first->getValue() == node.value) {
      // @n.first is LLVMNode * that we want
      LLVMNode *head = n.first;

      // Run "BFS" to traverse dependencies and see if there is
      // among them some function call.
      // TODO: write propper BFS (with visited nodes checking and stuff).
      std::vector<LLVMNode *> queue;
      queue.push_back(head);
      while (false == queue.empty()) {
        // Pop @current and investigate it first.
        LLVMNode *curr = queue.back();
        queue.pop_back();

        Value *curr_val = curr->getValue();
        if (isa<CallInst>(curr_val)) {
          // OK, we have found function call that is a data
          // dependency for @node.
          CallInst *curr_call_inst = cast<CallInst>(curr_val);
          Function *curr_fcn = curr_call_inst->getCalledFunction();
          std::string curr_fcn_name = curr_fcn->getName();
          // Add this function to the @path (if it is not already
          // there).
          bool curr_fcn_in_path = false;
          for (Function *path_fcn : path) {
            std::string path_fcn_name = path_fcn->getName();
            if (curr_fcn_name == path_fcn_name) {
              curr_fcn_in_path = true;
            }
          }
          if (false == curr_fcn_in_path) {
            path.push_back(curr_fcn);
          }
        }

        // Add neighbours to the queue.
        for (LLVMNode *neighbor : apex_dg.node_data_dependencies_map[curr]) {
          queue.push_back(neighbor);
        }
      }
    }
  }
}
/// Same as apexDgNodeResolveDependencies, but for reverse data dependencies
// TODO: Refactor this and apexDgNodeResolveDependencies() into one function.
void APEXPass::apexDgNodeResolveRevDependencies(
    std::vector<Function *> &path, APEXDependencyGraph &apex_dg,
    const APEXDependencyNode &node) {

  for (auto n : apex_dg.node_rev_data_dependencies_map) {
    if (n.first->getValue() == node.value) {
      // @n.first is LLVMNode * that we want
      LLVMNode *head = n.first;

      // Run "BFS" to traverse dependencies and see if there is
      // among them some function call.
      // TODO: write propper BFS (with visited nodes checking and stuff).
      std::vector<LLVMNode *> queue;
      queue.push_back(head);
      while (false == queue.empty()) {
        // Pop @current and investigate it first.
        LLVMNode *curr = queue.back();
        queue.pop_back();

        Value *curr_val = curr->getValue();
        if (isa<CallInst>(curr_val)) {
          // OK, we have found function call that is a data
          // dependency for @node.
          CallInst *curr_call_inst = cast<CallInst>(curr_val);
          Function *curr_fcn = curr_call_inst->getCalledFunction();
          std::string curr_fcn_name = curr_fcn->getName();
          // Add this function to the @path (if it is not already
          // there).
          bool curr_fcn_in_path = false;
          for (Function *path_fcn : path) {
            std::string path_fcn_name = path_fcn->getName();
            if (curr_fcn_name == path_fcn_name) {
              curr_fcn_in_path = true;
            }
          }
          if (false == curr_fcn_in_path) {
            path.push_back(curr_fcn);
          }
        }

        // Add neighbours to the queue.
        for (LLVMNode *neighbor :
             apex_dg.node_rev_data_dependencies_map[curr]) {
          queue.push_back(neighbor);
        }
      }
    }
  }
}

/// Investigates possible dependencies that may functions in @path have.
/// In case there are some dependencies, @path is updated (that is,
/// additional functions are added).
void APEXPass::updatePathAddDependencies(std::vector<Function *> &path,
                                         APEXDependencyGraph &apex_dg) {
  logPrintUnderline("updatePathAddDependencies(): Calculating and possibly "
                    "adding dependencies to @path.");

  // Make list out of path, so we can have pop_front().
  std::list<Function *> invesigation_list(path.begin(), path.end());

  while (false == invesigation_list.empty()) {
    logPrintFlat("path: ");
    functionVectorFlatPrint(path);
    logPrintFlat("investigation_list: ");
    functionListFlatPrint(invesigation_list);

    // Take front element from @path and investigate dependencies.
    Function *current_fcn = invesigation_list.front();
    invesigation_list.

        pop_front();

    std::string current_fcn_name = current_fcn->getName();
    logPrint("investigating: " + current_fcn_name);

    // Iterate over @current_fcn instructions
    for (auto &BB : *current_fcn) {
      for (auto &I : BB) {

        // We care only about call instructions.
        if (false == isa<CallInst>(I)) {
          continue;
        }

        // Ok, @I is call instruction.
        CallInst *call_inst = cast<CallInst>(&I);
        Function *called_fcn = call_inst->getCalledFunction();

        std::string called_fcn_name = called_fcn->getName();

        logPrint("- " + current_fcn_name + " is calling: " + called_fcn_name);

        // Dependencies resolution for @I from here.
        for (Function *path_function : path) {
          if (called_fcn != path_function) {
            continue;
          }
          // Ok, so @current_fcn is calling @called_fcn and @called_fcn
          // is also in @path.

          logPrint("-- " + called_fcn_name + " is in @path");
          // Now, investigate if call to @called_fcn has some data
          // dependencies (that are function calls), and add those
          // potential function calls to the path, so the function
          // that is going to be called is not removed.

          // Figure out which APEXDependencyFunction in @apex_dg is
          // @current_fcn.
          for (APEXDependencyFunction &apex_dg_fcn : apex_dg.functions) {
            std::string apex_dg_fcn_name = apex_dg_fcn.value->getName();
            if (current_fcn_name != apex_dg_fcn_name) {
              continue;
            }

            // We have our APEXDependencyFunction.
            // Now we need to find which node (AKA instruction) is calling
            // @called_fcn.
            for (APEXDependencyNode &node : apex_dg_fcn.nodes) {
              // We care only about call instructions.
              if (false == isa<CallInst>(node.value)) {
                continue;
              }

              // Ok, so @node.value is some call instruction. Now figure out
              // if it is the one that calls @called_fcn.
              CallInst *node_inst = cast<CallInst>(node.value);
              Function *node_called_fcn = node_inst->getCalledFunction();
              if (node_called_fcn != called_fcn) {
                continue;
              }

              apexDgNodeResolveRevDependencies(path, apex_dg, node);
              apexDgNodeResolveDependencies(path, apex_dg, node);
            }
          }
        }
      }
    }
    logPrint("");
  }
}

/// Take @node and find all data dependencies that form the chain.
/// Store these dependencies in the @dependencies vector.
// TODO: Refactor this function. It is copy-paste code atm.
void APEXPass::apexDgFindDataDependencies(
    APEXDependencyGraph &apex_dg, LLVMNode &node,
    std::vector<LLVMNode *> &data_dependencies,
    std::vector<LLVMNode *> &rev_data_dependencies) {
  {
    std::vector<LLVMNode *> queue;
    queue.push_back(&node);
    while (false == queue.empty()) {
      // Pop @curr and investigate it first.
      LLVMNode *curr = queue.back();
      queue.pop_back();

      // Store @curr if it is not already stored.
      bool new_dependency = true;
      for (LLVMNode *node : data_dependencies) {
        if (curr == node) {
          new_dependency = false;
        }
      }
      if (true == new_dependency) {
        data_dependencies.push_back(curr);
      }

      // Add neighbours to the queue.
      for (LLVMNode *neighbor : apex_dg.node_data_dependencies_map[curr]) {
        queue.push_back(neighbor);
      }
    }
  }
  {
    std::vector<LLVMNode *> queue;
    queue.push_back(&node);
    while (false == queue.empty()) {
      // Pop @curr and investigate it first.
      LLVMNode *curr = queue.back();
      queue.pop_back();

      // Store @curr if it is not already stored.
      bool new_dependency = true;
      for (LLVMNode *node : rev_data_dependencies) {
        if (curr == node) {
          new_dependency = false;
        }
      }
      if (true == new_dependency) {
        rev_data_dependencies.push_back(curr);
      }

      // Add neighbours to the queue.
      for (LLVMNode *neighbor : apex_dg.node_rev_data_dependencies_map[curr]) {
        queue.push_back(neighbor);
      }
    }
  }
}

// Module utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Goes over all functions in module. If there is some function in module
/// that is not in the @path, put it for removal along it all its dependencies.
void APEXPass::moduleRemoveFunctionsNotInPath(Module &M,
                                              APEXDependencyGraph &apex_dg,
                                              std::vector<Function *> &path) {
  logPrintUnderline("moduleRemoveFunctionsNotInPath(): Removing functions that "
                    "do not affect @path.");

  logPrint("Collecting functions that are going to be removed:");
  std::vector<Function *> functions_to_be_removed;
  {
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

    logPrintFlat("- path: ");
    functionVectorFlatPrint(path);
    logPrintFlat("- functions_to_be_removed: ");
    functionVectorFlatPrint(functions_to_be_removed);
  }

  logPrint("\nCollecting instruction dependencies for each function that has "
           "been marked for removal:");
  std::vector<LLVMNode *> collected_dependencies;
  {
    for (Function *fcn : functions_to_be_removed) {
      std::string fcn_id = fcn->getGlobalIdentifier();
      std::vector<LLVMNode *> dependencies;
      int unique_dependencies = 0;

      functionCollectDependencies(apex_dg, fcn_id, dependencies);

      // Append to the @collected_dependencies instruction that is not
      // already in the collection.
      for (LLVMNode *d : dependencies) {
        bool d_already_in = false;
        for (LLVMNode *stored_d : collected_dependencies) {
          if (d == stored_d) {
            d_already_in = true;
          }
        }
        if (false == d_already_in) {
          unique_dependencies += 1;
          collected_dependencies.push_back(d);
        }
      }

      logPrint("- function: " + fcn_id);
      logPrint("-- instruction dependencies count: " +
               std::to_string(dependencies.size()));
      logPrint("-- unique instruction dependencies count: " +
               std::to_string(unique_dependencies));
    }
  }

  logPrint("\nRemoving collected instruction dependencies:");
  {
    int removed_inst_count = 0;

    for (LLVMNode *n : collected_dependencies) {
      Instruction *inst = cast<Instruction>(n->getValue());

      if (false == inst->use_empty()) {
        // I'm not sure if this is even necessary.
        inst->replaceAllUsesWith(UndefValue::get(inst->getType()));
      }
      inst->eraseFromParent();
      removed_inst_count += 1;
    }

    logPrint("- removed instructions count: " +
             std::to_string(removed_inst_count));
  }

  logPrint("\nRemoving marked function bodies:");
  {
    int removed_fcn_count = 0;

    for (Function *f : functions_to_be_removed) {
      f->eraseFromParent();
      removed_fcn_count += 1;
    }

    logPrint("- removed functions count: " + std::to_string(removed_fcn_count));
  }
}
