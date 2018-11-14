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
  logPrintUnderline("APEXPass START.");

  APEXDependencyGraph apex_dg;
  LLVMDependenceGraph dg;
  std::vector<std::pair<Function *, std::vector<Function *>>> call_graph;
  //  std::vector<Function *> path;
  std::string source_function_id =
      "main";                          // TODO: Figure out entry automatically?
  std::string target_function_id = ""; // Will be properly initialized later.
  std::vector<const Instruction *> target_instructions;
  std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
      function_dependency_blocks;
  std::map<std::vector<LLVMNode *>, std::vector<const Function *>>
      blocks_functions_callgraph;

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Initial module dump.");
    logDumpModule(M);
  }

  logPrintUnderline("Parsing command line arguments.");
  moduleParseCmdLineArgsOrDie();

  logPrintUnderline("Locating target instructions.");
  moduleFindTargetInstructionsOrDie(M, ARG_FILE, ARG_LINE, target_instructions);
  target_function_id =
      target_instructions.back()->getFunction()->getGlobalIdentifier();
  logPrint("\nTarget function: " + target_function_id);

  logPrintUnderline(
      "Initializing dg. Calculating control and data dependencies.");
  dgInit(M, dg);

  logPrintUnderline("Extracting data from dg. Building apex dependency graph.");
  apexDgInit(apex_dg);

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Printing data dependencies from apex_dg.");
    apexDgPrintDataDependenciesCompact(apex_dg);
  }

  //  logPrintUnderline(
  //      "Building callgraph with root at function: " + source_function_id +
  //      ".");
  //  createCallGraphOrDie(M, source_function_id, call_graph);
  //
  //  if (VERBOSE_DEBUG) {
  //    logPrintUnderline("Printing callgraph.");
  //    printCallGraph(call_graph);
  //  }
  //
  //  logPrintUnderline("Finding path from @" + source_function_id + " to @" +
  //                    target_function_id);
  //  findPathOrDie(call_graph, source_function_id, target_function_id, path);
  //
  //  logPrintUnderline("Printing path from @" + source_function_id + " to @" +
  //                    target_function_id);
  //  printPath(path, source_function_id, target_function_id);

  logPrintUnderline("Constructing function dependency blocks.");
  apexDgComputeFunctionDependencyBlocks(M, apex_dg, function_dependency_blocks);

  //  if (VERBOSE_DEBUG) {
  logPrintUnderline("Printing calculated function dependency blocks.");
  apexDgPrintFunctionDependencyBlocks(function_dependency_blocks);
  //  }

  logPrintUnderline("Constructing dependency blocks to functions callgraph.");
  apexDgConstructBlocksFunctionsCallgraph(function_dependency_blocks,
                                          blocks_functions_callgraph);

  //  if (VERBOSE_DEBUG) {
  logPrintUnderline("Printing dependency block to functions callgraph.");
  apexDgPrintBlocksFunctionsCallgraph(blocks_functions_callgraph);
  //  }

  logPrintUnderline("Finding path from @" + source_function_id + " to @" +
                    target_function_id + ".");
  // @path is the representation of computed execution path
  // It holds pair of function and dependency block from function through which
  // is the execution "flowing".
  std::vector<std::vector<LLVMNode *>> path;

  // @block_path is used to store path from @source_function to each node.
  std::map<std::vector<LLVMNode *> *, std::vector<std::vector<LLVMNode *>>>
      block_path;
  {
    std::vector<std::vector<LLVMNode *> *> queue;
    std::vector<std::vector<LLVMNode *> *> visited;

    // Initially push blocks from source function into the @queue.
    for (auto &block :
         function_dependency_blocks[M.getFunction(source_function_id)]) {
      queue.push_back(&block);
    }

    while (false == queue.empty()) {
      const auto current_ptr = queue.back();
      queue.pop_back();
      visited.push_back(current_ptr);

      // Find if there are calls to other functions that come from the current
      // block.
      for (const auto &called_function :
           blocks_functions_callgraph[*current_ptr]) {
        // Get blocks for those functions that are called.
        for (auto &neighbour_block :
             function_dependency_blocks[called_function]) {
          // What we do here is that we copy the path from the current
          // block, append current node to it and assign this path to
          // each neighbour of current.
          block_path[&neighbour_block] = block_path[current_ptr];
          block_path[&neighbour_block].push_back(*current_ptr);

          bool block_already_visited = false;
          for (const auto &visited_block_ptr : visited) {
            if (visited_block_ptr == &neighbour_block) {
              block_already_visited = true;
              break;
            }
          }
          // Add non-visited blocks from called function into the queue.
          if (false == block_already_visited) {
            queue.push_back(&neighbour_block);
          }
        }
      }
    }
  }

  // Finding target block. That is, block where target instruction are.
  // We use heuristic and take last instruction from the @target_instructions.
  // The reason is that inside @target_instructions may be more instructions
  // than present in target block (especially at the beginning), so
  // TODO: This is segfaulting when @target_block is pointer.
  // TODO: Leaving it as a value for now.
  std::vector<LLVMNode *> target_block;
  for (auto block :
       function_dependency_blocks[M.getFunction(target_function_id)]) {
    for (auto node_ptr : block) {
      if (target_instructions.back() == node_ptr->getValue()) {
        // Check the target line, just in case.
        if (std::to_string(
                target_instructions.back()->getDebugLoc().getLine()) ==
            ARG_LINE) {
          target_block = block;
        }
      }
    }
  }
  if (target_block.empty()) {
    logPrint("ERROR: Could not find target block! Make sure target "
             "instructions are not dead code (e.g. in the uncalled function");
    exit(FATAL_ERROR);
  }

  logPrint("Reconstructing block path:");
  for (const auto bp : block_path) {
    // Comparing values can be slow, but blocks should not be really huge.
    if (*bp.first == target_block) {
      path = bp.second;
    }
  }
  path.push_back(target_block);

  for (const auto block : path) {
    Instruction *i = cast<Instruction>(block.front()->getValue());
    logPrint("- BLOCK FROM: " + i->getFunction()->getGlobalIdentifier());
    for (const auto node : block) {
      node->getValue()->dump();
    }
    logPrint("");
  }

  /*
    logPrintUnderline("Dependency resolver: START");
    // Investigate each function in @path.
    for (auto &fcn_in_path : path) {
      logPrint("FCN: " + fcn_in_path->getGlobalIdentifier());
      for (auto &BB : *fcn_in_path) {
        // Go over instructions for each function in @path.
        for (auto &I : BB) {
          if (false == isa<CallInst>(I)) {
            continue;
          }
          // Ok, @I is call instruction.
          // If @I calls something in @path, compute dependencies and include
          // new stuff to @path.
          CallInst *call_inst = cast<CallInst>(&I);
          Function *called_fcn = call_inst->getCalledFunction();
          logPrintFlat("- @I calling: " + called_fcn->getGlobalIdentifier());

          // We care if @I is calling function that is in the @path.
          if (false == functionInPath(called_fcn, path)) {
            // Do not investigate @I if it is calling function OUTSIDE PATH.
            logPrint(" (not in path)");
            continue;
          }

          // We care if @I is calling non protected function.
          if (functionIsProtected(called_fcn)) {
            logPrint(" (protected)");
            continue;
          }

          logPrint(" (IN path)");

          // Find @I in the @apex_dg.
          logPrint("  - finding @I in @apex_dg");
          APEXDependencyNode *I_apex = nullptr;
          for (APEXDependencyFunction apex_fcn : apex_dg.functions) {
            std::string apex_fcn_name = apex_fcn.value->getName();
            if (apex_fcn_name == fcn_in_path->getGlobalIdentifier()) {
              for (APEXDependencyNode apex_node : apex_fcn.nodes) {
                if (isa<CallInst>(apex_node.value)) {
                  CallInst *node_inst = cast<CallInst>(apex_node.value);
                  Function *node_called_fcn = node_inst->getCalledFunction();
                  if (node_called_fcn == called_fcn) {
                    I_apex = &apex_node;
                    logPrintFlat("  - found @I: ");
                    I_apex->value->dump();
                    goto I_FOUND; // Get out of the loops.
                  }
                }
              }
            }
          }
          if (nullptr == I_apex) {
            logPrint("ERROR: Could not find @I in @apex_dg!");
            exit(-1);
          }

        I_FOUND:
          // Now, check if @I has some data dependencies that are fcn calls.
          std::vector<LLVMNode *> data_dependencies;
          std::vector<LLVMNode *> rev_data_dependencies;
          LLVMNode *I_apex_node = I_apex->node;
          apexDgFindDataDependencies(apex_dg, *I_apex_node, data_dependencies,
                                     rev_data_dependencies);

          logPrint("   - @I data dependencies:\n");
          updatePathAddDependencies(data_dependencies, path);
          logPrint("");
          logPrint("   - @I reverse data dependencies:\n");
          updatePathAddDependencies(rev_data_dependencies, path);
          logPrint("");
        }
      }
      // TODO: one iteration of dep resolving is done

      logPrint("\n---\n");
    }
    printPath(path, source_function_id, target_function_id);
  */

  // TODO: ERROR: inlinable function call in a function with debug info must
  // have a !dbg location
  // TODO: Need to setup DebugLoc to the instruction?
  //  logPrintUnderline("Inserting exit call before @target_instruction.");
  //  moduleInsertExitAfterTarget(M, path, target_function);

  //  logPrintUnderline("Removing functions (along with dependencies) that do
  //  not affect the @path and are not in the @PROTECTED_FCNS.");
  //  moduleRemoveFunctionsNotInPath(M, apex_dg, path);

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Final module dump.");
    logDumpModule(M);
  }

  logPrintUnderline("APEXPass END.");
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

/// Returns true if function @F is protected.
/// That is, part of the @PROTECTED_FCNS
/// Protected functions will not be removed at the end of the APEXPass.
bool APEXPass::functionIsProtected(const Function *F) {
  for (std::string fcn_id : PROTECTED_FCNS) {
    if (F->getGlobalIdentifier() == fcn_id) {
      return true;
    }
  }
  return false;
}

/// Returns true if function @F is in @path.
bool APEXPass::functionInPath(Function *F,
                              const std::vector<Function *> &path) {
  for (Function *fcn : path) {
    if (fcn == F) {
      return true;
    }
  }
  return false;
}

// Callgraph utilities
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Creates callgraph from module @M, starting from the function with global id
/// specified in the @root.
///
/// Callgraph is saved in the vector @callgraph as the pairs of the following
/// format:
/// <caller function, vector of called functions>.
///
/// Dies if:
/// - unable to find function with @root global ID.
/// - unable to collect target callees when building callgraph.
int APEXPass::createCallGraphOrDie(
    const Module &M, const std::string &root,
    std::vector<std::pair<Function *, std::vector<Function *>>> &callgraph) {

  Function *root_fcn = M.getFunction(root);
  if (nullptr == root_fcn) {
    logPrint("ERROR : Root function should not be nullptr!");
    exit(FATAL_ERROR);
  }

  std::vector<Function *> queue;
  queue.push_back(root_fcn);

  while (false == queue.empty()) {
    Function *node = queue.back();
    queue.pop_back();

    // Find call functions that are called by the @node.
    std::vector<Function *> node_callees;
    if (functionGetCallees(node, node_callees) < 0) {
      logPrint("ERROR: Failed to collect target callees!");
      exit(FATAL_ERROR);
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

  logPrint("- done");
  return 0;
}

/// Prints callgraph that is stored in the @callgraph.
void APEXPass::printCallGraph(
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
    logPrint(caller + callees);
  }
}

/// Uses BFS to find path in the @callgraph from @source to @target
/// (both are global Function IDs).
///
/// Result is stored in the @final_path.
///
/// Dies if unable to find source node.
int APEXPass::findPathOrDie(
    const std::vector<std::pair<Function *, std::vector<Function *>>>
        &callgraph,
    const std::string &source, const std::string &target,
    std::vector<Function *> &final_path) {

  logPrint("Searching for the source node:");
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
      logPrint("ERROR: Could not find source node!");
      exit(FATAL_ERROR);
    } else {
      logPrint("- done");
    }
  }

  logPrint("Running BFS to find path from @" + source + " to @" + target + ":");
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

  logPrint("Reconstructing and storing @" + source + " -> @" + target +
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

/// Prints path from @source to @target (both are function global IDs).
void APEXPass::printPath(const std::vector<Function *> &path,
                         const std::string &source, const std::string &target) {
  for (auto &node : path) {
    logPrint(node->getGlobalIdentifier());
  }
}

// dg utilities
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Initializes dg and calculates control & data dependencies.
void APEXPass::dgInit(Module &M, LLVMDependenceGraph &dg) {
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

/// Extracts data from dg and stores them into @apex_dg structures.
///
/// Caution: @dgInit() has to be called before @apexDgInit(), dg needs to be
///          initialized in for this to properly work, otherwise
///          CF map will be empty.
void APEXPass::apexDgInit(APEXDependencyGraph &apex_dg) {
  const std::map<llvm::Value *, LLVMDependenceGraph *> &CF =
      dg::getConstructedFunctions(); // Need to call this (dg reasons).

  logPrint("Initializing @apex_dg structures:");
  for (auto &function_dg : CF) {
    APEXDependencyFunction apex_function;
    apex_function.value = function_dg.first;

    for (auto &value_block : function_dg.second->getBlocks()) {
      // We do not care about blocks, we care about function:node
      // relationship.
      for (auto &node : value_block.second->getNodes()) {
        // Create node (AKA instruction) structure and fill it with
        // control & data dependencies to other nodes.
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

  logPrint("Storing data dependencies into @apex_dg structures:");
  for (auto &apex_dg_function : apex_dg.functions) {
    for (auto &node : apex_dg_function.nodes) {
      apex_dg.node_data_dependencies_map[node.node] = node.data_dependencies;
      apex_dg.node_rev_data_dependencies_map[node.node] =
          node.rev_data_dependencies;
      apex_dg.node_function_map[node.node] = &apex_dg_function;
    }
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

/// Pretty prints @apex_dg, but only with data dependencies.
void APEXPass::apexDgPrintDataDependenciesCompact(
    APEXDependencyGraph &apex_dg) {
  for (APEXDependencyFunction &function : apex_dg.functions) {
    std::string fcn_name = function.value->getName();
    logPrint("\n===> " + fcn_name);
    for (APEXDependencyNode &node : function.nodes) {

      logPrintFlat("\n- node:");
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

/// Amazing, we have instruction that is calling some function.
/// Now check if this instruction has dependencies.
///
/// If it has dependencies and any of those is function call
/// that is not in the @path, add function that it calls to
/// the path.
// TODO: Refactor this function.
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

/// Take @node and find all data dependencies that form the chain.
/// Store these dependencies in the @dependencies vector.
// TODO: Refactor this function. It is copy-paste code atm.
void APEXPass::apexDgFindDataDependencies(
    APEXDependencyGraph &apex_dg, LLVMNode &node,
    std::vector<LLVMNode *> &data_dependencies,
    std::vector<LLVMNode *> &rev_data_dependencies) const {
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
      if (new_dependency && curr != &node) {
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
      if (new_dependency && curr != &node) {
        rev_data_dependencies.push_back(curr);
      }

      // Add neighbours to the queue.
      for (LLVMNode *neighbor : apex_dg.node_rev_data_dependencies_map[curr]) {
        queue.push_back(neighbor);
      }
    }
  }
}

/// Goes over @dependencies and checks whether there are any function calls
/// to functions outside @path. If yes, function that is being called
/// is pushed to the @path.
///
/// Note:
/// @dependencies are dependencies for specific LLVMNode computed
/// by @apexDgFindDataDependencies().
void APEXPass::updatePathAddDependencies(
    const std::vector<LLVMNode *> &dependencies,
    std::vector<Function *> &path) {
  for (LLVMNode *dd : dependencies) {
    dd->getValue()->dump();
    if (isa<CallInst>(dd->getValue())) {
      logPrintFlat("Dependency is CALL to: ");

      CallInst *dd_call_inst = cast<CallInst>(dd->getValue());
      Function *dd_called_fcn = dd_call_inst->getCalledFunction();
      logPrintFlat(dd_called_fcn->getGlobalIdentifier());

      if (functionIsProtected(dd_called_fcn)) {
        logPrint(" (protected)");
        continue;
      }
      logPrint("");

      if (false == functionInPath(dd_called_fcn, path)) {
        logPrint(dd_called_fcn->getGlobalIdentifier() + ": not in path!");
        logPrint("- Pushing to path: " + dd_called_fcn->getGlobalIdentifier());
        path.push_back(dd_called_fcn);
      } else {
        logPrint(dd_called_fcn->getGlobalIdentifier() + ": already in path");
      }
    }
  }
}

/// Finds instruction @I in the @apex_dg.
///
/// Returns @APEXDependencyNode, that is @I equivalent in the @apex_dg.
///
/// Dies if unable to find @I in the @apex_dg.
void APEXPass::apexDgGetNodeOrDie(const APEXDependencyGraph &apex_dg,
                                  const Instruction *const I,
                                  APEXDependencyNode &node) {
  for (APEXDependencyFunction apex_fcn : apex_dg.functions) {
    for (APEXDependencyNode &apex_node : apex_fcn.nodes) {
      if (apex_node.value == I) {
        node = apex_node;
        return;
      }
    }
  }
  logPrintFlat(
      "ERROR: Could not find the following instruction in the @apex_dg."
      "Maybe it is in function that @dg did not computed dependencies"
      "(because it is not used, etc.)");
  I->dump();
  exit(FATAL_ERROR);
}

/// We go over functions that are in @apex_dg and compute for each function
/// dependencies between instructions. Instructions that are linked via data
/// dependencies are stored in the @functions_blocks map.
void APEXPass::apexDgComputeFunctionDependencyBlocks(
    const Module &M, APEXDependencyGraph &apex_dg,
    std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
        &function_dependency_blocks) {
  for (auto &F : M) {
    logPrint("Constructing dependency blocks for: " + F.getGlobalIdentifier());

    if (functionIsProtected(&F)) {
      logPrint("- function is protected");
      continue;
    }

    bool function_in_apex_dg = false;
    for (APEXDependencyFunction &apex_fcn : apex_dg.functions) {
      if (&F == apex_fcn.value) {
        function_in_apex_dg = true;
      }
    }
    if (false == function_in_apex_dg) {
      logPrint("- function not in @apex_dg (probably not used, etc.)");
      continue;
    }

    std::vector<std::vector<LLVMNode *>> function_blocks;
    unsigned num_fcn_instructions = 0;

    for (auto &BB : F) {

      std::vector<LLVMNode *> visited;
      for (auto &I : BB) {
        num_fcn_instructions++;

        std::vector<LLVMNode *> new_block;
        APEXDependencyNode apex_node;
        apexDgGetNodeOrDie(apex_dg, &I, apex_node);

        bool go_to_next_instruction = false;

        // Check if @I is already in some stored block.
        for (auto &block : function_blocks) {
          for (LLVMNode *dep_block_node : block) {
            if (apex_node.node == dep_block_node) {
              // @apex_node is already in some @block.
              // Go investigate directly next instruction.
              go_to_next_instruction = true;
            }
          }
        }
        if (go_to_next_instruction) {
          continue;
        }

        std::vector<LLVMNode *> queue;
        queue.push_back(apex_node.node);

        while (false == queue.empty()) {
          LLVMNode *current = queue.back();
          queue.pop_back();

          bool already_visited = false;
          for (LLVMNode *visited_node : visited) {
            if (current == visited_node) {
              already_visited = true;
              break;
            }
          }
          if (already_visited) {
            continue;
          }

          visited.push_back(current);

          // Get dependency chain for @current;
          std::vector<LLVMNode *> data_dependencies;
          std::vector<LLVMNode *> rev_data_dependencies;
          apexDgFindDataDependencies(apex_dg, *apex_node.node,
                                     data_dependencies, rev_data_dependencies);
          // I think it is enough to consider only data dependencies and
          // do not care about reverse data dependencies.
          // Reverse data dependencies contain instructions that are outside
          // current function and that created problems.
          std::vector<LLVMNode *> neighbours = data_dependencies;

          bool current_already_stored = false;

          for (LLVMNode *neighbour : neighbours) {
            bool neighbour_already_stored = false;
            for (auto &block : function_blocks) {
              for (LLVMNode *node : block) {
                if (neighbour == node) {
                  neighbour_already_stored = true;

                  for (auto &b : function_blocks) {
                    for (LLVMNode *n : b) {
                      if (current == n) {
                        current_already_stored = true;
                      }
                    }
                  }

                  if (false == current_already_stored) {
                    block.push_back(current);
                    current_already_stored = true;
                  }
                }
              }
            }
            if (false == neighbour_already_stored) {
              queue.push_back(neighbour);
            }
          }

          if (false == current_already_stored) {
            new_block.push_back(current);
          }
        }

        if (new_block.size() > 0) {
          function_blocks.push_back(new_block);
        }
      }
    }

    // Checking if we did not miss any instruction.
    unsigned num_dep_blocks_instructions = 0;
    for (auto &block : function_blocks) {
      num_dep_blocks_instructions += block.size();
    }
    assert(num_fcn_instructions == num_dep_blocks_instructions);
    logPrint("- done: " + std::to_string(num_fcn_instructions) +
             " instructions in blocks");

    // TODO: Instructions are not in the original order. This may be a problem.
    // TODO: Sort them here if it would be.

    // Everything should be OK, store computed blocks into storage.
    function_dependency_blocks[&F] = function_blocks;
  }
}

/// Pretty prints @functions_blocks map.
void APEXPass::apexDgPrintFunctionDependencyBlocks(
    const std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
        &function_dependency_blocks) {
  for (auto &function_blocks : function_dependency_blocks) {
    std::string fcn_id = function_blocks.first->getGlobalIdentifier();
    logPrint("FUNCTION: " + fcn_id);
    for (auto &block : function_blocks.second) {
      logPrint("- BLOCK:");
      for (auto &node : block) {
        logPrintFlat("  ");
        node->getValue()->dump();
      }
    }
    logPrint("");
  }
};

/// Takes @function_dependency_blocks that were computed by
/// @apexDgComputeFunctionDependencyBlocks() and constructs callgraph where
/// key is some block and value is vector of functions that this block may call.
void APEXPass::apexDgConstructBlocksFunctionsCallgraph(
    const std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
        &function_dependency_blocks,
    std::map<std::vector<LLVMNode *>, std::vector<const Function *>>
        &blocks_functions_callgraph) {
  for (auto &function_blocks : function_dependency_blocks) {
    for (auto &block : function_blocks.second) {
      for (LLVMNode *node : block) {
        if (isa<CallInst>(node->getValue())) {
          const CallInst *call_inst = cast<CallInst>(node->getValue());
          const Function *called_fcn = call_inst->getCalledFunction();
          // Store call edge from block to function.
          auto &functions = blocks_functions_callgraph[block];
          functions.push_back(called_fcn);
          blocks_functions_callgraph[block] = functions;
        }
      }
    }
  }
  logPrint("- done");
}

/// Pretty prints dependency blocks to functions callgraph.
void APEXPass::apexDgPrintBlocksFunctionsCallgraph(
    const std::map<std::vector<LLVMNode *>, std::vector<const Function *>>
        &blocks_functions_callgraph) {
  for (const auto &block_functions : blocks_functions_callgraph) {
    const Instruction *node_inst =
        cast<Instruction>(block_functions.first.front()->getValue());
    logPrint("BLOCK: in " + node_inst->getFunction()->getGlobalIdentifier());
    for (const auto &node : block_functions.first) {
      node->getValue()->dump();
    }
    logPrint("Calls:");
    for (const auto &function : block_functions.second) {
      logPrint("- " + function->getGlobalIdentifier());
    }
    logPrint("");
  }
}

// Module utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Parses command line arguments from opt.
///
/// Dies if arguments are missing or are in from format.
void APEXPass::moduleParseCmdLineArgsOrDie(void) {
  // TODO: Add handling in case of missing/wrong values.
  logPrint("file = " + ARG_FILE);
  logPrint("line = " + ARG_LINE);
}

/// Tries to find instructions that are located at @file and @line.
/// (this can be one or multiple instructions).
///
/// Dies if unable to find instructions.
void APEXPass::moduleFindTargetInstructionsOrDie(
    Module &M, const std::string &file, const std::string &line,
    std::vector<const Instruction *> &target_instructions) {
  for (const auto &F : M) {
    for (const auto &BB : F) {
      for (const auto &I : BB) {
        if (auto const inst_loc_ptr = I.getDebugLoc()) {
          const std::string line = std::to_string(inst_loc_ptr->getLine());
          const std::string file = inst_loc_ptr->getFilename();
          // std::string dir = inst_loc_ptr->getDirectory();
          if (file == ARG_FILE && line == ARG_LINE) {
            // Found instruction that matches ARG_FILE+ARG_LINE
            target_instructions.push_back(&I);
          }
        }
      }
    }
  }

  if (target_instructions.empty()) {
    logPrint("ERROR: Could not locate target instructions!");
    exit(FATAL_ERROR);
  }

  logPrint("Instructions at: file = " + ARG_FILE + ", line = " + ARG_LINE);
  logPrint("");
  for (const auto inst_ptr : target_instructions) {
    inst_ptr->dump();
  }
}

/// Goes over all functions in module. If there is some function in module
/// that is not in the @path, put it for removal along it all its dependencies.
/// Note: Functions that are in @PROTECTED_FCNS, will NOT be removed.
void APEXPass::moduleRemoveFunctionsNotInPath(Module &M,
                                              APEXDependencyGraph &apex_dg,
                                              std::vector<Function *> &path) {
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
      // @module_fcn is inside @PROTECTED_FCNS, do not remove it.
      // Inside @PROTECTED_FCNS could be some function from external library,
      // or debug functions or whatever that we do not want to optimize out.
      for (std::string &protected_fcn_id : PROTECTED_FCNS) {
        if (protected_fcn_id == module_fcn.getGlobalIdentifier()) {
          module_fcn_to_be_removed = false;
        }
      }

      if (module_fcn_to_be_removed) {
        functions_to_be_removed.push_back(&module_fcn);
      }
    }

    logPrintFlat("- path: ");
    functionVectorFlatPrint(path);
    logPrintFlat("- functions to be removed: ");
    functionVectorFlatPrint(functions_to_be_removed);
    logPrintFlat("- protected functions: ");
    for (std::string &protected_fcn_id : PROTECTED_FCNS) {
      logPrintFlat(protected_fcn_id + ", ");
    }
    logPrint("");
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

/// Finds FUNCTION with @target_id in @path. Takes predecessor of FUNCTION from
/// path. In this predecessor, finds the call to the FUNCTION and inserts after
/// this instruction, call to lib_exit() from lib.c
void APEXPass::moduleInsertExitAfterTarget(Module &M,
                                           const std::vector<Function *> &path,
                                           const std::string &target_id) {
  logPrint("Getting/picking callsite for @" + target_id);
  Function *target_fcn = M.getFunction(target_id);
  {
    if (nullptr == target_fcn) {
      logPrint("ERROR: Could not get target function from module.");
      exit(-1);
    }

    std::vector<Function *> target_fcn_callers;
    if (functionGetCallers(target_fcn, target_fcn_callers) != 0) {
      logPrint("ERROR: Could not get callers for target function.");
    }

    logPrint("Collected @" + target_id + " callers:");
    for (Function *f : target_fcn_callers) {
      logPrint("- " + f->getGlobalIdentifier());
    }
  }

  logPrint("\nPicking one caller:");
  Function *chosen_caller = nullptr;
  {
    // Ok, so we have @target_fcn_callers AKA functions that call @target.
    // There can be multiple callers, so we need to decide which one to
    // pick.
    // We will pick one that is immediately before @target in @path, e.g.:
    // If, @path = [main, n, y], @target = y, so we will pick @n.
    for (int i = 0; i < path.size(); ++i) {
      if ((path[i] == target_fcn) && (i > 0)) {
        chosen_caller = path[i - 1];
      }
    }
    if (nullptr == chosen_caller) {
      logPrint("ERROR: Could not find chosen caller.");
      exit(-1);
    }
    logPrint("- chosen caller id: " + chosen_caller->getGlobalIdentifier());
  }

  logPrint("\nFinding chosen call instruction to target function inside @" +
           chosen_caller->getGlobalIdentifier() + ":");
  Instruction *chosen_instruction = nullptr;
  {
    // Now that we have function that should call @target_function, we search
    // inside for call instruction to @target_function.
    for (auto &BB : *chosen_caller) {
      for (auto &&I : BB) {
        if (isa<CallInst>(I)) {
          Function *called_fcn = cast<CallInst>(I).getCalledFunction();
          if (called_fcn == target_fcn) {
            chosen_instruction = &I;
            break;
          }
        }
      }
    }
    if (nullptr == chosen_instruction) {
      logPrint("ERROR: Could not find chosen instruction.");
      exit(-1);
    }
    logPrint("- call instruction to target function found");
  }

  logPrint("\nSearching for insertion point (that is, instruction after chosen "
           "instruction):");
  Instruction *insertion_point = nullptr;
  {
    // We have @chosen_instruction, but we want to find instruction that sits
    // right after @chosen_instruction. This way, we can insert exit call
    // after @chosen_instruction and not before.
    for (inst_iterator I = inst_begin(chosen_caller),
                       E = inst_end(chosen_caller);
         I != E; ++I) {
      Instruction *current_inst = &(*I);
      if (current_inst == chosen_instruction && I != E) {
        I++;
        insertion_point = &(*I);
      }
    }
    if (nullptr == insertion_point) {
      logPrint("ERROR: could not find insertion point!");
      exit(-1);
    }
    logPrintFlat("- insertion point found: ");
    insertion_point->dump();
  }

  logPrint("\nInserting call instruction to lib_exit() after chosen "
           "instruction (that is, before insertion point).");
  {
    // Create vector of types and put one integer into this vector.
    std::vector<Type *> params_tmp = {Type::getInt32Ty(M.getContext())};
    // We need to convert vector to ArrayRef.
    ArrayRef<Type *> params = makeArrayRef(params_tmp);

    // Create function that returns void and has parameters that are
    // specified in the @params (that is, one Int32).
    FunctionType *fType =
        FunctionType::get(Type::getVoidTy(M.getContext()), params, false);

    // Load exit function into variable.
    Constant *temp = M.getOrInsertFunction("lib_exit", fType);
    if (nullptr == temp) {
      logPrint("ERROR: lib_exit function is not in symbol table.");
      exit(-1);
    }
    Function *exit_fcn = cast<Function>(temp);
    logPrint("- loaded function: " + exit_fcn->getGlobalIdentifier());

    // Create exit call instruction.
    Value *exit_arg_val = ConstantInt::get(Type::getInt32Ty(M.getContext()), 9);
    ArrayRef<Value *> exit_params = makeArrayRef(exit_arg_val);
    // CallInst::Create build call instruction to @exit_fcn that has
    // @exit_params. Empty string "" means that the @exit_call_inst will not
    // have return variable.
    // @exit_call_inst is inserted BEFORE @chosen_instruction.
    CallInst *exit_call_inst =
        CallInst::Create(exit_fcn, exit_params, "", insertion_point);

    if (nullptr == exit_call_inst) {
      logPrint("ERROR: could not create lib_exit call instruction.");
      exit(-1);
    }
    logPrintFlat("- lib_exit call instruction created: ");
    exit_call_inst->dump();
  }
}
