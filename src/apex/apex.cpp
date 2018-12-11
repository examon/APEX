// Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
//
// Published under Apache 2.0 license.
// See LICENSE for details.

// What is this:
// This file implements APEX as LLVM Pass (APEXPass). See build_and_run.sh for
// information about how to run APEXPass.

// Current known limitations:
// - target only non global int

// TODO: user I.getprevnode() or I.getnextnode() to for injecting
// TODO: remove unused includes

#include "apex.h"

/// Running on each module.
bool APEXPass::runOnModule(Module &M) {
  logPrintUnderline("APEXPass START.");


  if (VERBOSE_DEBUG) {
    logPrintUnderline("Initial module dump.");
    logDumpModule(M);
  }

  logPrintUnderline("Parsing command line arguments.");
  moduleParseCmdLineArgsOrDie();

  logPrintUnderline("Locating target instructions.");
  moduleFindTargetInstructionsOrDie(M, ARG_FILE, ARG_LINE);

  logPrintUnderline("Collecting protected functions.");
  collectProtectedFunctions(M);

  logPrintUnderline(
      "Initializing dg. Calculating control and data dependencies.");
  dgInit(M);

  logPrintUnderline("Extracting data from dg. Building apex dependency graph.");
  apexDgInit();

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Printing data dependencies from apex_dg.");
    apexDgPrintDependenciesCompact();
  }

  logPrintUnderline("Constructing function dependency blocks.");
  apexDgComputeFunctionDependencyBlocks(M);

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Printing calculated function dependency blocks.");
    apexDgPrintFunctionDependencyBlocks();
  }

  logPrintUnderline("Constructing dependency blocks to functions call graph.");
  apexDgConstructBlocksFunctionsCallgraph();

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Printing dependency block to functions call graph.");
    apexDgPrintBlocksFunctionsCallgraph();
  }

  logPrintUnderline("Finding path from @" + source_function_id_ + " to @" +
                    target_function_id_ + ".");
  findPath(M);

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Printing path from @" + source_function_id_ + " to @" +
                      target_function_id_ + ".");
    printPath();
  }

  logPrintUnderline("Removing functions and dependency blocks that do not "
                    "affect calculated path.");
  removeUnneededStuff(M);

  logPrintUnderline("Injecting exit and extract calls.");
  moduleInjectExitExtract(M);

  logPrintUnderline("Stripping debug symbols from every function in module.");
  stripAllDebugSymbols(M);

  if (VERBOSE_DEBUG) {
    logPrintUnderline("Final module dump.");
    logDumpModule(M);
  }

  logPrintUnderline("APEXPass END.");
  return true;
}

// Logging utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Simple logging print with newline.
void APEXPass::logPrint(const std::string &message) {
  errs() << message + "\n";
}

/// Simple logging print with newline.
void APEXPass::logPrintDbg(const std::string &message) {
  if (VERBOSE_DEBUG) {
    errs() << message + "\n";
  }
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
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Returns true if function @F is protected,
/// Protected functions will not be removed at the end of the APEXPass.
bool APEXPass::functionIsProtected(const Function *F) {
  for (std::string fcn_id : protected_functions_) {
    if (F->getGlobalIdentifier() == fcn_id) {
      return true;
    }
  }
  return false;
}

// Callgraph utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Finds blocks in the source function and starts exploring
/// @blocks_functions_callgraph from these blocks using BFS.
///
/// Computes path from source function to target function.
/// Stores results in the @path. @path contains dependency
/// blocks that are on the execution flow.
void APEXPass::findPath(const Module &M) {
  // @block_path is used to store path from @source_function to each node.
  std::map<DependencyBlock *, std::vector<DependencyBlock>> block_path;
  {
    std::vector<DependencyBlock *> queue;
    std::vector<DependencyBlock *> visited;

    // Initially push blocks from source function into the @queue.
    for (auto &block :
         function_dependency_blocks_[M.getFunction(source_function_id_)]) {
      queue.push_back(&block);
    }

    while (false == queue.empty()) {
      const auto current_ptr = queue.back();
      queue.pop_back();
      visited.push_back(current_ptr);

      // Find if there are calls to other functions that come from the current
      // block.
      for (const auto &called_function :
           blocks_functions_callgraph_[*current_ptr]) {
        // Get blocks for those functions that are called.
        for (auto &neighbour_block :
             function_dependency_blocks_[called_function]) {
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
  // We use heuristic and take last instruction from the @target_instructions_.
  // The reason is that inside @target_instructions_ may be more instructions
  // than present in target block (especially at the beginning), so
  DependencyBlock *target_block = nullptr;
  for (auto &block :
       function_dependency_blocks_[M.getFunction(target_function_id_)]) {
    for (const auto &node_ptr : block) {
      if (target_instructions_.back() == node_ptr->getValue()) {
        // Check the target line, just in case.
        if (std::to_string(
                target_instructions_.back()->getDebugLoc().getLine()) ==
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
      path_ = bp.second;
    }
  }
  path_.push_back(*target_block);
  logPrint("- done");
}

/// Prints path in the @path.
void APEXPass::printPath() {
  for (const auto block : path_) {
    Instruction *i = cast<Instruction>(block.front()->getValue());
    logPrint("- BLOCK FROM: " + i->getFunction()->getGlobalIdentifier());
    for (const auto node : block) {
      node->getValue()->dump();
    }
    logPrint("");
  }
}

// dg utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Initializes dg and calculates control & data dependencies.
void APEXPass::dgInit(Module &M) {
  // The reasoning why everything below needs to be here:
  //
  // In order to get data dependencies, I've replicated
  // what llvm-dg-dump tool is doing, so for details, check:
  // dg/tools/llvm-dg-dump.cpp
  LLVMPointerAnalysis *pta = new LLVMPointerAnalysis(&M);
  pta->run<analysis::pta::PointsToFlowInsensitive>();
  dg_.build(&M, pta);
  analysis::rd::LLVMReachingDefinitions rda(&M, pta);
  rda.run<analysis::rd::ReachingDefinitionsAnalysis>();
  // rda.run<analysis::rd::SemisparseRda>(); // This is alternative to above
  // ^^
  LLVMDefUseAnalysis dua(&dg_, &rda, pta);
  dua.run();
  dg_.computeControlDependencies(CD_ALG::CLASSIC);

  logPrint("- done");
}

// apex dg utilities
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/// Extracts data from @dg_ and stores them into @apex_dg_ structures.
///
/// Caution: @dgInit() has to be called before @apexDgInit(), dg needs to be
///          initialized in for this to properly work, otherwise
///          CF map will be empty.
void APEXPass::apexDgInit() {
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

        // Store data & control dependencies from @node to @apex_node.
        apexDgGetBlockNodeInfo(apex_node, node);

        apex_function.nodes.push_back(apex_node);
      }
    }
    apex_dg_.functions.push_back(apex_function);
  }
  logPrint("- done");

  logPrint("\nStoring data and control dependencies into @apex_dg structures:");
  for (auto &apex_dg_function : apex_dg_.functions) {
    for (auto &node : apex_dg_function.nodes) {
      // Data & reverse data dependencies.
      apex_dg_.node_data_dependencies_map[node.node] = node.data_dependencies;
      apex_dg_.node_rev_data_dependencies_map[node.node] =
          node.rev_data_dependencies;

      // Control & reverse control dependencies.
      apex_dg_.node_control_dependencies_map[node.node] =
          node.control_depenencies;
      apex_dg_.node_rev_control_dependencies_map[node.node] =
          node.rev_control_depenencies;

      // Map function to node.
      apex_dg_.node_function_map[node.node] = &apex_dg_function;
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

/// Pretty prints @apex_dg_, but only with data dependencies.
void APEXPass::apexDgPrintDependenciesCompact() {
  for (APEXDependencyFunction &function : apex_dg_.functions) {
    std::string fcn_name = function.value->getName();

    logPrint("\n===> " + fcn_name);
    for (APEXDependencyNode &node : function.nodes) {

      logPrintFlat("\n- node:");
      node.value->dump();

      // Pretty print data dependencies.
      logPrint("[dd]:");
      for (LLVMNode *dd : node.data_dependencies) {
        dd->getValue()->dump();
      }
    }
  }
}

/// Take @node and find all data dependencies that form the chain.
/// Store these dependencies in the @dependencies vector.
// TODO: Refactor this function. It is copy-paste code!
void APEXPass::apexDgFindDataDependencies(
    LLVMNode &node, std::vector<LLVMNode *> &data_dependencies,
    std::vector<LLVMNode *> &rev_data_dependencies) {
  {
    std::vector<LLVMNode *> visited;
    std::vector<LLVMNode *> queue;
    queue.push_back(&node);
    while (false == queue.empty()) {
      // Pop @curr and investigate it first.
      LLVMNode *curr = queue.back();
      queue.pop_back();

      bool already_visited = false;
      for (LLVMNode *visited_node : visited) {
        if (curr == visited_node) {
          already_visited = true;
          break;
        }
      }
      if (already_visited) {
        continue;
      }
      visited.push_back(curr);

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
      for (LLVMNode *neighbor : apex_dg_.node_data_dependencies_map[curr]) {
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

      std::vector<LLVMNode *> visited;
      bool already_visited = false;
      for (LLVMNode *visited_node : visited) {
        if (curr == visited_node) {
          already_visited = true;
          break;
        }
      }
      if (already_visited) {
        continue;
      }
      visited.push_back(curr);

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
      for (LLVMNode *neighbor : apex_dg_.node_rev_data_dependencies_map[curr]) {
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

/// We go over functions that are in @apex_dg_ and compute for each function
/// dependencies between instructions. Instructions that are linked via data
/// dependencies are stored in the @functions_blocks map.
void APEXPass::apexDgComputeFunctionDependencyBlocks(const Module &M) {
  logPrint("Constructing dependency blocks:");

  for (auto &F : M) {
    logPrintDbg("Constructing dependency blocks for: " +
                F.getGlobalIdentifier());

    if (functionIsProtected(&F)) {
      logPrintDbg("- function is protected");
      continue;
    }

    bool function_in_apex_dg = false;
    for (APEXDependencyFunction &apex_fcn : apex_dg_.functions) {
      if (&F == apex_fcn.value) {
        function_in_apex_dg = true;
      }
    }
    if (false == function_in_apex_dg) {
      logPrintDbg("- function not in @apex_dg (probably not used, etc.)");
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
        apexDgGetNodeOrDie(apex_dg_, &I, apex_node);

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
          apexDgFindDataDependencies(*apex_node.node, data_dependencies,
                                     rev_data_dependencies);
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
//    assert(num_fcn_instructions == num_dep_blocks_instructions);
    logPrintDbg("- done: " + std::to_string(num_fcn_instructions) +
                " instructions in blocks");

    // Everything should be OK, store computed blocks into storage.
    function_dependency_blocks_[&F] = function_blocks;
  }
  logPrint("- done");

  // Instructions in stored in the @new_blocks are not in the same order
  // as they are in the @F.
  // We need to put them in the correct order and store that result back
  // to the @function_dependency_blocks.
  logPrint("\nSorting function dependency blocks:");
  std::map<const Function *, std::vector<std::vector<LLVMNode *>>>
      function_dependency_blocks_sorted;
  for (const auto &function_blocks : function_dependency_blocks_) {
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
//      assert(new_block_sorted.size() == block.size());
      function_dependency_blocks_sorted[function_blocks.first].push_back(
          new_block_sorted);
    }
  }
  function_dependency_blocks_ = function_dependency_blocks_sorted;
  logPrint("- done");
}

/// Pretty prints @functions_blocks map.
void APEXPass::apexDgPrintFunctionDependencyBlocks() {
  for (auto &function_blocks : function_dependency_blocks_) {
    std::string fcn_id = function_blocks.first->getGlobalIdentifier();
    logPrint("FUNCTION: " + fcn_id);
    for (auto &block : function_blocks.second) {
      logPrint("- COMPONENT:");
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
void APEXPass::apexDgConstructBlocksFunctionsCallgraph() {
  for (auto &function_blocks : function_dependency_blocks_) {
    for (auto &block : function_blocks.second) {
      for (LLVMNode *node : block) {

        if (isa<CallInst>(node->getValue())) {
          const CallInst *call_inst = cast<CallInst>(node->getValue());
          const Function *called_fcn = call_inst->getCalledFunction();

          // Store call edge from block to function.
          auto &functions = blocks_functions_callgraph_[block];
          functions.push_back(called_fcn);
          blocks_functions_callgraph_[block] = functions;
        }
      }
    }
  }
  logPrint("- done");
}

/// Pretty prints dependency blocks to functions callgraph.
void APEXPass::apexDgPrintBlocksFunctionsCallgraph() {
  for (const auto &block_functions : blocks_functions_callgraph_) {
    const Instruction *node_inst =
        cast<Instruction>(block_functions.first.front()->getValue());
    logPrint("COMPONENT: in " +
             node_inst->getFunction()->getGlobalIdentifier());
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
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

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
void APEXPass::moduleFindTargetInstructionsOrDie(Module &M,
                                                 const std::string &file,
                                                 const std::string &line) {
  logPrintDbg("ARG_FILE:" + ARG_FILE);
  logPrintDbg("ARG_LINE:" + ARG_LINE);
  for (const auto &F : M) {
    for (const auto &BB : F) {
      for (const auto &I : BB) {
        if (auto const debug_info = I.getDebugLoc()) {
          const std::string line = std::to_string(debug_info->getLine());
          const std::string file = debug_info->getFilename();
          // std::string dir = inst_loc_ptr->getDirectory();
          I.dump();
          logPrintDbg("line:" + line);
          logPrintDbg("file:" + file);
          logPrintDbg("---");
          if (file == ARG_FILE && line == ARG_LINE) {
            // Found instruction that matches ARG_FILE+ARG_LINE
            target_instructions_.push_back(&I);
          }
        }
      }
    }
  }

  if (target_instructions_.empty()) {
    logPrint("ERROR: Could not locate target instructions!");
    exit(FATAL_ERROR);
  }

  logPrint("Instructions at: file = " + ARG_FILE + ", line = " + ARG_LINE);
  logPrint("");
  for (const auto inst_ptr : target_instructions_) {
    inst_ptr->dump();
  }

  target_function_id_ =
      target_instructions_.back()->getFunction()->getGlobalIdentifier();
  logPrint("\nTarget function: " + target_function_id_);
}

/// Injects _apex_exit() call at the end of the @target_instructions_.
/// Injects lib_dump_int() call before lib_exit().
void APEXPass::moduleInjectExitExtract(Module &M) {
  logPrint("Supplied target instructions from: " +
           target_instructions_.back()->getFunction()->getGlobalIdentifier());
  for (const auto &target_instruction : target_instructions_) {
    if (nullptr == target_instruction) {
      continue;
    }
    target_instruction->dump();
  }

  logPrint("\nSetting injection point:");
  Instruction *injection_point =
      const_cast<Instruction *>(target_instructions_.back());
  logPrintFlat("-");
  injection_point->dump();

  logPrint("\nInjecting call instruction to _apex_exit():");
  {
    // Create vector of types and put one integer into this vector.
    std::vector<Type *> params_tmp = {Type::getInt32Ty(M.getContext())};
    // We need to convert vector to ArrayRef.
    ArrayRef<Type *> params = makeArrayRef(params_tmp);

    // Create function that returns void and has parameters that are
    // specified in the @params (that is, one Int32).
    FunctionType *fcn_type =
        FunctionType::get(Type::getVoidTy(M.getContext()), params, false);

    // Load exit function into variable.
    Constant *temp = M.getOrInsertFunction("_apex_exit", fcn_type);
    if (nullptr == temp) {
      logPrint("ERROR: lib_exit function is not in symbol table.");
      exit(-1);
    }
    Function *_apex_exit_fcn = cast<Function>(temp);
    logPrint("- loaded function: " + _apex_exit_fcn->getGlobalIdentifier());

    // Create exit call instruction.
    Value *_apex_exit_fcn_arg_val =
        ConstantInt::get(Type::getInt32Ty(M.getContext()), APEX_DONE);
    ArrayRef<Value *> _apex_exit_fcn_params =
        makeArrayRef(_apex_exit_fcn_arg_val);
    // CallInst::Create build call instruction to @exit_fcn that has
    // @_apex_exit_fcn_params. Empty string "" means that the
    // @_apex_exit_call_inst will not have return variable.
    CallInst *_apex_exit_call_inst =
        CallInst::Create(_apex_exit_fcn, _apex_exit_fcn_params, "");

    // If the @insertion point is terminator, insert @_apex_exit_call_inst
    // before it. Otherwise, basic blocks will not end with terminator.
    // TODO: Will this inconsistency cause problems?
    if (injection_point->isTerminator()) {
      logPrintFlat("- @injection_point is terminator, inserting " +
                   _apex_exit_fcn->getGlobalIdentifier() + " before: ");
      injection_point->dump();
      _apex_exit_call_inst->insertBefore(injection_point);
    } else {
      logPrintFlat("- @injection_point is NOT terminator, inserting " +
                   _apex_exit_fcn->getGlobalIdentifier() + " after: ");
      injection_point->dump();
      _apex_exit_call_inst->insertAfter(injection_point);
    }

    // Final check.
    if (nullptr == _apex_exit_call_inst) {
      logPrint("ERROR: could not create " +
               _apex_exit_fcn->getGlobalIdentifier() + " call instruction.");
      exit(-1);
    }
    logPrintFlat("- " + _apex_exit_fcn->getGlobalIdentifier() +
                 " call instruction created: ");
    _apex_exit_call_inst->dump();

    // Set insertion point to the exit call we just inserted.
    // All new instructions are going to be inserted before exit call.
    injection_point = _apex_exit_call_inst;
  }

  logPrint("\nInjecting call instruction to _apex_extract_int():");
  {
    // Load _apex_extract_int() from lib.c and save it.
    Instruction *last_target =
        const_cast<Instruction *>(target_instructions_.back());
    std::vector<Type *> params_tmp = {PointerType::getInt32Ty(M.getContext())};
    ArrayRef<Type *> params = makeArrayRef(params_tmp);
    FunctionType *fcn_type =
        FunctionType::get(Type::getVoidTy(M.getContext()), params, false);
    Constant *temp = M.getOrInsertFunction("_apex_extract_int", fcn_type);
    Function *_apex_extract_int_fcn = cast<Function>(temp);
    logPrint("- loaded function: " +
             _apex_extract_int_fcn->getGlobalIdentifier());

    logPrintFlat("- last of the target functions: ");
    last_target->dump();

    if (false == isa<StoreInst>(last_target)) {
      logPrint("ERROR: Invalid target instruction! It has to be StoreInst!");
      exit(FATAL_ERROR);
    }

    if (2 != last_target->getNumOperands()) {
      logPrint("ERROR: Invalid target instruction! It has to have 2 operands!");
      exit(FATAL_ERROR);
    }

    // Example of instruction: store i32 %call, i32* %f, allign 4
    // @_apex_extract_int_arg would be second operand: i32* %f
    Value *_apex_extract_int_arg =
        last_target->getOperand(last_target->getNumOperands() - 1);
    logPrintFlat("- last of the target instructions, second op: ");
    _apex_extract_int_arg->dump();

    // Now we need to load that pointer to i32 into classic i32.
    // Because @lib_extract_int takes i32 instead of i32*.
    LoadInst *_apex_extract_int_arg_loadinst = new LoadInst(
        _apex_extract_int_arg, "_apex_extract_int_arg", injection_point);
    logPrintFlat("- created load instruction: ");
    _apex_extract_int_arg_loadinst->dump();

    // Now make callinst to _apex_extract_int with the correct argument
    // (that is i32 and not i32*).
    std::vector<Value *> dump_params = {_apex_extract_int_arg_loadinst};
    CallInst *lib_extract_int_fcn_callinst =
        CallInst::Create(_apex_extract_int_fcn, dump_params, "");

    logPrintFlat("- " + _apex_extract_int_fcn->getGlobalIdentifier() +
                 " call instruction created: ");
    lib_extract_int_fcn_callinst->dump();
    lib_extract_int_fcn_callinst->insertBefore(injection_point);
  }
}

/// Figures out what DependencyBlocks and functions to remove and removes them.
void APEXPass::removeUnneededStuff(Module &M) {

  // Mapping from function to dependency blocks. Blocks belong to function.
  // We are going to keep these function:blocks.
  std::map<const Function *, std::set<DependencyBlock>> function_blocks_to_keep;

  // @queue and @visited are going to be used for BFS search of dependencies.
  std::vector<DependencyBlock> queue;
  std::vector<DependencyBlock> visited;

  logPrint("Checking if we have any branching dependent on the @path:");
  {
    // We will use @path_container as a collecting container for new blocks.
    // We will append these new blocks after we end the iteration over @path_.
    std::vector<DependencyBlock> path_container;

    for (auto &path_block : path_) {
      printPath();
      // Set of basic blocks that nodes inside @path_node belong to.
      std::set<const BasicBlock *> block_bbs;

      bool block_has_inst_in_if_bb = false;

      logPrintDbg("- investigating block, collecting basic blocks:");
      for (auto &node : path_block) {
        const Instruction *node_inst = cast<Instruction>(node->getValue());
        std::string bb_name = node_inst->getParent()->getName();
        block_bbs.insert(node_inst->getParent());

        if (VERBOSE_DEBUG) {
          logPrintFlat("  [" + bb_name + "]:");
          node->getValue()->dump();
        }

        // Get first 3 chars of the basic block name.
        // We are looking for "if.then" basic blocks.
        if (bb_name.substr(0, 3) == "if.") {
          block_has_inst_in_if_bb = true;
        }
      }

      logPrintDbg("  - collected basic blocks:");
      for (const auto &bb : block_bbs) {
        std::string bb_name = bb->getName();
        logPrintDbg("    - " + bb_name);
      }

      // @path_block is good to go, no branching is dependent on it.
      if (false == block_has_inst_in_if_bb) {
        logPrintDbg("  - block has no instruction in if.* basic block");
        continue;
      }
      logPrintDbg("  - block has some instruction in if.* basic block, check "
                  "more:");

      // We know that some instructions from @block_path are in the BB
      // that is designated as if.*.
      // Find branch instruction that services this BB and add block
      // associated with this branch instruction to the @path.

      // Function that @path_block belongs to.
      const Function *block_function =
          cast<Instruction>(path_block.front()->getValue())->getFunction();

      logPrintDbg("    - block in fcn: " +
                  block_function->getGlobalIdentifier());
      logPrintDbg("    - checking fcn for br instructions:");
      for (const auto &BB : *block_function) {
        for (const auto &I : BB) {

          if (isa<BranchInst>(I)) {
            if (VERBOSE_DEBUG) {
              logPrintFlat("      - ok, we have br inst: ");
              I.dump();
            }

            for (unsigned i = 0; i < I.getNumOperands(); ++i) {
              Value *op = I.getOperand(i);
              std::string op_name = op->getName();
              logPrintDbg("         - op: " + op_name);

              for (const auto &bb : block_bbs) {
                // We have branch instruction which can send execution into
                // block that is currently in @path.
                // Get block associated with this branch instruction and put
                // this block into the @path.
                if (bb == op) {
                  logPrintDbg("           - bb == op");

                  // Find wich block contains our branch instruction.
                  for (const auto &fcn_block :
                       function_dependency_blocks_[block_function]) {
                    for (const auto &node : fcn_block) {
                      Instruction *node_inst =
                          cast<Instruction>(node->getValue());
                      if (node_inst == &I) {
                        logPrintDbg("           - adding block associated with "
                                    "the branch instruction to the @path");
                        path_container.push_back(fcn_block);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    path_.insert(path_.end(), path_container.begin(), path_container.end());
    printPath();
    logPrint("- done");
  }

  if (VERBOSE_DEBUG) {
    logPrint("\nPrinting @path:");
    printPath();
  }

  logPrint("\nComputing what dependency blocks we want to keep:");
  {
    logPrint("- marking every block from @path as to keep");
    for (const auto &block : path_) {
      // Mark as visited to make sure we do not process this block in BFS.
      visited.push_back(block);

      const Instruction *front_inst =
          cast<Instruction>(block.front()->getValue());
      function_blocks_to_keep[front_inst->getFunction()].insert(block);
    }

    logPrint("- setting up initial queue for BFS search");
    // Go over @path and figure out if there are any calls outside the @path.
    // If there are, put those called blocks for investigation into the @queue.
    for (const auto &path_block : path_) {
      for (const auto &called_function :
           blocks_functions_callgraph_[path_block]) {

        // Is @called_function part of the path?
        bool called_function_part_of_path = false;

        for (const auto &b : path_) {
          const Instruction *b_inst = cast<Instruction>(b.front()->getValue());
          if (b_inst->getFunction() == called_function) {
            called_function_part_of_path = true;
            break;
          }
        }

        if (false == called_function_part_of_path) {
          for (auto const &called_function_block :
               function_dependency_blocks_[called_function]) {
            queue.push_back(called_function_block);
          }
        }
      }
    }

    logPrint("- running BFS");
    // Run BFS from queue and add everything for keeping that is not visited.
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
      for (const auto &called_function : blocks_functions_callgraph_[current]) {
        for (const auto &block : function_dependency_blocks_[called_function]) {

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
    logPrint("- done");
  }

  logPrint("\nCollecting everything that we do not want to keep:");
  std::set<DependencyBlock> blocks_to_remove;
  std::set<const Function *> functions_to_remove;
  {
    for (const auto &module_function : M.getFunctionList()) {
      // We do not investigate protected functions (do not want to remove them).
      if (functionIsProtected(&module_function)) {
        logPrintDbg("- fnc is protected: " +
                    module_function.getGlobalIdentifier());
        continue;
      }

      // Check if @module_function has entry in the @function_blocks_to_keep
      // (that is, there is @module_function with some of its blocks in stored
      // in the @function_blocks_to_keep).
      if (function_blocks_to_keep.count(&module_function) > 0) {
        logPrintDbg("- fnc to +++ KEEP +++: " +
                    module_function.getGlobalIdentifier());

        // Go over all blocks in the @module_function.
        for (auto const &block :
             function_dependency_blocks_[&module_function]) {
          // Check if @block has entry in the
          // @function_blocks_to_keep[&module_function]
          if (function_blocks_to_keep[&module_function].count(block) > 0) {
            // @block is stored and thus we are going to keep it.
            logPrintDbg("  - block to +++ KEEP +++");
          } else {
            // @block is not stored and we should consider to mark it for
            // removal.
            bool node_has_branch_inst = false;
            for (const auto &node : block) {
              if (isa<BranchInst>(node->getValue())) {
                node_has_branch_inst = true;
              }
            }
            if (node_has_branch_inst) {
              // @node has branch instruction, we will not mark this node
              // for removal just to be safe.
              logPrintDbg("  - block to +++ KEEP +++: has branch inst");
            } else {
              blocks_to_remove.insert(block);
              logPrintDbg("  - block to remove:");
            }
          }

          if (VERBOSE_DEBUG) {
            for (auto const &node_ptr : block) {
              logPrintFlat("   ");
              node_ptr->getValue()->dump();
            }
          }
        }
      } else {
        logPrintDbg("- fnc to remove: " +
                    module_function.getGlobalIdentifier());
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

        // Do not erase instruction that is inside @target_instructions_.
        // We need those instructions intact.
        bool inst_is_target = false;
        for (const auto &target_inst : target_instructions_) {
          if (target_inst == inst) {
            inst_is_target = true;
            break;
          }
        }
        if (false == inst_is_target) {
          // To be sure that we do not erase one of the target instructions.
          inst->eraseFromParent();
        }
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
  logPrint("- done");
}

/// Strips debug symbols from every function in module @M.
///
/// We do this because LLVM verifier would blow up after we inserted into the
/// module instructions without debug symbols.
/// This way, module will be consistent.
void APEXPass::stripAllDebugSymbols(Module &M) {
  for (auto &F : M) {
    llvm::stripDebugInfo(F);
  }
  logPrint("- done");
}


/// Goes over all functions in the module and adds functions
/// that are only declarations to the @protected_functions_.
/// We do not want to remove declarations.
void APEXPass::collectProtectedFunctions(Module &M) {
  for (const auto &F: M) {
    if (VERBOSE_DEBUG) {
      logPrint(F.getGlobalIdentifier());
    }
    if (F.isDeclaration()) {
      logPrintDbg("- is declaration");
      protected_functions_.push_back(F.getGlobalIdentifier());
    }
  }
  logPrint("- done");
}