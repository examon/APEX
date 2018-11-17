// Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
//
// Published under Apache 2.0 license.
// See LICENSE for details.

// What is this:
// This file implements APEX as LLVM Pass (APEXPass). See build_and_run.sh for
// information about how to run APEXPass.

// Current known limitations:
// - not possible to target global variable

// TODO: Create aliases for long type defs.
// TODO: Move instance variables from runOnModule to APEXPass


#include "apex.h"

/// Running on each module.
bool APEXPass::runOnModule(Module &M) {
  logPrintUnderline("APEXPass START.");

  APEXDependencyGraph apex_dg;
  LLVMDependenceGraph dg;

  // @path is the representation of computed execution path.
  // It holds pair of function and dependency block from function through which
  // is the execution "flowing".
  std::vector<DependencyBlock> path;

  std::string source_function_id =
      "main";                          // TODO: Figure out entry automatically?
  std::string target_function_id = ""; // Will be properly initialized later.

  std::vector<const Instruction *> target_instructions;
  std::map<const Function *, std::vector<DependencyBlock>>
      function_dependency_blocks;
  std::map<DependencyBlock, std::vector<const Function *>>
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

  logPrintUnderline("Constructing function dependency blocks.");
  apexDgComputeFunctionDependencyBlocks(M, apex_dg, function_dependency_blocks);

  logPrintUnderline("Printing calculated function dependency blocks.");
  apexDgPrintFunctionDependencyBlocks(function_dependency_blocks);

  logPrintUnderline("Constructing dependency blocks to functions callgraph.");
  apexDgConstructBlocksFunctionsCallgraph(function_dependency_blocks,
                                          blocks_functions_callgraph);

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Printing dependency block to functions callgraph.");
    apexDgPrintBlocksFunctionsCallgraph(blocks_functions_callgraph);
  }

  logPrintUnderline("Finding path from @" + source_function_id + " to @" +
                    target_function_id + ".");
  findPath(M, blocks_functions_callgraph, function_dependency_blocks,
           target_instructions, source_function_id, target_function_id, path);

  logPrintUnderline("Printing path from @" + source_function_id + " to @" +
                    target_function_id + ".");
  printPath(path);

  logPrintUnderline("Removing functions and dependency blocks that do not "
                    "affect calculated path.");
  removeUnneededStuff(M, path, blocks_functions_callgraph,
                      function_dependency_blocks, source_function_id,
                      target_function_id);

  // TODO: ERROR: inlinable function call in a function with debug info must
  // TODO:   have a !dbg location
  // TODO: Need to setup DebugLoc to the instruction?
  //  logPrintUnderline("Inserting exit call before @target_instruction.");
  //  moduleInsertExitAfterTarget(M, path, target_function);

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


// Callgraph utilities
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Finds blocks in the source function and starts exploring
/// @blocks_functions_callgraph from these blocks using BFS.
///
/// Computes path from source function to target function.
/// Stores results in the @path. @path contains dependency
/// blocks that are on the execution flow.
void APEXPass::findPath(
    Module &M,
    std::map<DependencyBlock, std::vector<const Function *>>
        &blocks_functions_callgraph,
    std::map<const Function *, std::vector<DependencyBlock>>
        &function_dependency_blocks,
    const std::vector<const Instruction *> &target_instructions,
    const std::string &source_function_id,
    const std::string &target_function_id, std::vector<DependencyBlock> &path) {

  // @block_path is used to store path from @source_function to each node.
  std::map<DependencyBlock *, std::vector<DependencyBlock>> block_path;
  {
    std::vector<DependencyBlock *> queue;
    std::vector<DependencyBlock *> visited;

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
  DependencyBlock *target_block = nullptr;
  for (auto &block :
       function_dependency_blocks[M.getFunction(target_function_id)]) {
    for (const auto &node_ptr : block) {
      if (target_instructions.back() == node_ptr->getValue()) {
        // Check the target line, just in case.
        if (std::to_string(
                target_instructions.back()->getDebugLoc().getLine()) ==
            ARG_LINE) {
          target_block = &block;
        }
      }
    }
  }
  if (nullptr == target_block || target_block->empty()) {
    logPrint("ERROR: Could not find target block! Make sure target "
             "instructions are not dead code (e.g. in the uncalled function");
    exit(FATAL_ERROR);
  }

  logPrint("Reconstructing block path:");
  for (const auto &bp : block_path) {
    if (bp.first == target_block) {
      path = bp.second;
    }
  }
  path.push_back(*target_block);
  logPrint("- done");
}

/// Prints path in the @path.
void APEXPass::printPath(const std::vector<DependencyBlock> &path) {
  for (const auto block : path) {
    Instruction *i = cast<Instruction>(block.front()->getValue());
    logPrint("- BLOCK FROM: " + i->getFunction()->getGlobalIdentifier());
    for (const auto node : block) {
      node->getValue()->dump();
    }
    logPrint("");
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

/// Stores control/reverse_control/data/reverse_data dependencies of the @node
/// to the @apex_node.
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
      "Maybe it is in a function that @dg did not computed dependencies for"
      "(e.g. it is not used, etc.)");
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

    // Everything should be OK, store computed blocks into storage.
    function_dependency_blocks[&F] = function_blocks;
  }

  // Instructions in stored in the @new_blocks are not in the same order
  // as they are in the @F.
  // We need to put them in the correct order and store that result back
  // to the @function_dependency_blocks.
  logPrint("Sorting function dependency blocks:");
  std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
      function_dependency_blocks_sorted;
  for (const auto &function_blocks : function_dependency_blocks) {
    for (const auto &block : function_blocks.second) {
      std::vector<LLVMNode *> new_block_sorted;
      for (const auto &BB : *(function_blocks.first)) {
        for (const auto &I : BB) {
          for (const auto node_ptr : block) {
            if (node_ptr->getValue() == &I) {
              new_block_sorted.push_back(node_ptr);
            }
          }
        }
      }
      // Make sure we have not missed any instruction and sorted block has
      // every instruction that unsorted before storing @new_block_sorted.
      assert(new_block_sorted.size() == block.size());
      function_dependency_blocks_sorted[function_blocks.first].push_back(
          new_block_sorted);
    }
  }
  function_dependency_blocks = function_dependency_blocks_sorted;
  logPrint("- done");
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
      for (auto &I : BB) {
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

/// Figures out what DependencyBlocks and functions to remove and removes them.
void APEXPass::removeUnneededStuff(
    const Module &M, const std::vector<DependencyBlock> &path,
    std::map<DependencyBlock, std::vector<const Function *>>
        &blocks_functions_callgraph,
    std::map<const Function *, std::vector<DependencyBlock>>
        &function_dependency_blocks,
    const std::string &source_function_id,
    const std::string &target_function_id) {

  // Mapping from function to dependency blocks. Blocks belong to function.
  // We are going to keep these function:blocks.
  std::map<const Function *, std::set<DependencyBlock>> function_blocks_to_keep;

  logPrint("Computing what functions and dependency blocks we want to keep:");
  {
    std::vector<DependencyBlock> queue;
    std::vector<DependencyBlock> visited;

    // Reverse iterate @path so we begin BFS with the first node instead with
    // the last.
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
      queue.push_back(*it);
    }

    while (false == queue.empty()) {
      DependencyBlock current = queue.back();
      queue.pop_back();
      visited.push_back(current);

      // Figure out what is the parent function of @current.
      const Instruction *current_inst =
          cast<Instruction>(current.front()->getValue());

      // Store @current block associated with its parent function.
      function_blocks_to_keep[current_inst->getFunction()].insert(current);

      // Go over functions that are being called from the @current block.
      // Add them to the queue if they were not visited already.
      for (const auto &called_function : blocks_functions_callgraph[current]) {
        for (const auto &block : function_dependency_blocks[called_function]) {

          // Check if we already visited @block.
          bool block_already_visited = false;
          for (const auto &visited_block : visited) {
            if (block == visited_block) {
              block_already_visited = true;
              break;
            }
          }
          if (false == block_already_visited) {
            queue.push_back(block);
          }
        }
      }
    }

    // Pretty print functions and associated blocks that we want to keep.
    for (const auto &function_blocks : function_blocks_to_keep) {
      logPrint("FUNCTION: " + function_blocks.first->getGlobalIdentifier());
      for (const auto &block : function_blocks.second) {
        logPrint(" - block");
        // Printing nodes itself for debugging only.
        for (const auto &node_ptr : block) {
          node_ptr->getValue()->dump();
        }
        logPrint("");
      }
    }
  }

  logPrint("\nCollecting everything that we do not want to keep:");
  std::set<DependencyBlock> blocks_to_remove;
  std::set<const Function *> functions_to_remove;
  {
    for (const auto &module_function : M.getFunctionList()) {
      if (functionIsProtected(&module_function)) {
        logPrint("is protected: " + module_function.getGlobalIdentifier());
        continue;
      }

      // Check if @module_function is stored in the @function_blocks_to_keep.
      if (function_blocks_to_keep.count(&module_function) > 0) {
        logPrint("to keep: " + module_function.getGlobalIdentifier());

        for (auto const &block : function_dependency_blocks[&module_function]) {
          // Check if @block is stored in the set of DependencyBlocks inside
          // @function_dependency_blocks.
          if (function_blocks_to_keep[&module_function].count(block) > 0) {
            // @block is stored and thus we are going to keep it.
          } else {
            blocks_to_remove.insert(block);
            // logPrint(" - BLOCK DROP:");
            // for (auto const &node_ptr : block) {
            //  node_ptr->getValue()->dump();
            // }
          }
        }
      } else {
        logPrint("to remove: " + module_function.getGlobalIdentifier());
        functions_to_remove.insert(&module_function);
      }
    }
    logPrint("- done");
  }

  logPrint("\nRemoving unwanted blocks:");
  {
    for (auto const &block : blocks_to_remove) {
      // logPrint("Dropping block:"); // dbg

      // Go ahead and remove instructions stored the @block.
      for (auto const &node_ptr : block) {
        Instruction *inst = cast<Instruction>(node_ptr->getValue());
        // inst->dump(); // dbg

        // Watch out for terminators. Do not remove them!
        if (inst->isTerminator()) {
          // logPrint("IS TERMINATOR!"); // dbg
          continue;
        }

        if (false == inst->use_empty()) {
          // I'm not sure if this is even necessary.
          inst->replaceAllUsesWith(UndefValue::get(inst->getType()));
        }
        inst->eraseFromParent();
      }
      // logPrint(""); // dbg
    }
    logPrint("- done");
  }

  logPrint("\nRemoving unwanted functions:");
  {
    for (auto const &function : functions_to_remove) {
      // Need to get non-const Function ptr.
      Function *fcn_to_remove = M.getFunction(function->getGlobalIdentifier());

      logPrint("- removing: " + fcn_to_remove->getGlobalIdentifier());

      // Invalidate all function calls to @function. This is to make sure
      // that we do not have any dangling function calls to @function
      // after we remove @function. This would produce segfault.
      fcn_to_remove->replaceAllUsesWith(
          UndefValue::get(fcn_to_remove->getType()));

      // Invalidating instructions inside function that is going to be
      // removed is probably not needed. Still, better safe than sorry.
      for (auto &BB : *fcn_to_remove) {
        for (auto &I : BB) {
          if (false == I.use_empty()) {
            I.replaceAllUsesWith(UndefValue::get(I.getType()));
          }
        }
      }

      // Finally remove @function.
      fcn_to_remove->eraseFromParent();
    }
  }
}
