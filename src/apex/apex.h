// Created by Tomas Meszaros (exo at tty dot com, tmeszaro at redhat dot com)
//
// Published under Apache 2.0 license.
// See LICENSE for details.

// What is this?
// This is header file for apex.cpp

#pragma once

//#include <list>
//#include <map>
//#include <string>
//#include <vector>

#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/Metadata.h" // For StorageType
#include <llvm/ADT/APInt.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

// We need this for dg integration.
#include "analysis/PointsTo/PointsToFlowInsensitive.h"
#include "llvm/LLVMDependenceGraph.h"
#include "llvm/analysis/DefUse.h"

using namespace llvm;
using namespace dg;

/// For printing additional debug information.
bool VERBOSE_DEBUG = true;

/// Error code when we want to crash APEXPass.
int FATAL_ERROR = -1;

/// Exit code when we want to successfully exit.
int APEX_DONE = 0;

/// Function LLVMNodes connected via the data dependencies.
using DependencyBlock = std::vector<LLVMNode *>;

/// Protected functions IDs. These will not be removed by APEXPass.
std::vector<std::string> PROTECTED_FCNS = {
    // These belong to the _apex_exit function.
    "_apex_exit",
    "exit",

    // These belong to the _apex_extract_int function.
    "_apex_extract_int",
    "printf",

    // LLVM stuff
    "llvm.dbg.declare",
    "llvm.stackrestore",
    "llvm.stacksave",
};

/// Command line arguments for opt.
cl::opt<std::string> ARG_FILE(
    "file", cl::desc("Filename in relative path from to the launching script."),
    cl::value_desc("file name (e.g. code/main.c)"));

cl::opt<std::string> ARG_LINE("line",
                              cl::desc("Number representing line in the file."),
                              cl::value_desc("Source code line number."));

/// Node is usually line instruction of IR. Sometimes whole function.
struct APEXDependencyNode {
  LLVMNode *node;
  Value *value;
  std::vector<LLVMNode *> control_depenencies;
  std::vector<LLVMNode *> rev_control_depenencies;
  std::vector<LLVMNode *> data_dependencies;
  std::vector<LLVMNode *> rev_data_dependencies;
};

/// Function (can contain multiple basic blocks).
struct APEXDependencyFunction {
  Value *value;
  std::vector<APEXDependencyNode> nodes;
};

/// Graph: Consists of nodes that are functions.
struct APEXDependencyGraph {
  std::vector<APEXDependencyFunction> functions;
  // Mapping of nodes to dependencies & functions.
  std::map<LLVMNode *, std::vector<LLVMNode *>> node_data_dependencies_map;
  std::map<LLVMNode *, std::vector<LLVMNode *>> node_rev_data_dependencies_map;
  std::map<LLVMNode *, std::vector<LLVMNode *>> node_control_dependencies_map;
  std::map<LLVMNode *, std::vector<LLVMNode *>>
      node_rev_control_dependencies_map;
  std::map<LLVMNode *, APEXDependencyFunction *> node_function_map;
};

/// Actual APEX pass.
class APEXPass : public ModulePass {
public:
  static char ID;
  APEXPass() : ModulePass(ID) {}
  bool runOnModule(Module &M) override;

private:
  // Data members
  // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Source and target functions global IDs.
  std::string source_function_id_ = "main";
  std::string target_function_id_ = ""; // Will be properly initialized later.
  // Target instructions that correspond to the user input.
  std::vector<const Instruction *> target_instructions_;

  // Dependence graph: https://github.com/mchalupa/dg
  LLVMDependenceGraph dg_;

  // Holds all the necessary info about dependencies.
  APEXDependencyGraph apex_dg_;

  // @path_ is the representation of computed execution path.
  // It holds pair of function and dependency block from function through which
  // is the execution "flowing".
  std::vector<DependencyBlock> path_;

  std::map<const Function *, std::vector<DependencyBlock>>
      function_dependency_blocks_;

  std::map<DependencyBlock, std::vector<const Function *>>
      blocks_functions_callgraph_;

  // Member Functions
  // ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

  // Logging utilities.
  void logPrint(const std::string &message);
  void logPrintDbg(const std::string &message);
  void logPrintUnderline(const std::string &message);
  void logPrintFlat(const std::string &message);
  void logDumpModule(const Module &M);

  // Function utilities.
  bool functionIsProtected(const Function *F);

  // Callgraph utilities.
  void findPath(const Module &M);
  void printPath();

  // dg utilities.
  void dgInit(Module &M);

  // apex dg utilities.
  void apexDgInit();
  void apexDgGetBlockNodeInfo(APEXDependencyNode &apex_node, LLVMNode *node);
  void apexDgPrintDependenciesCompact();
  void
  apexDgFindDataDependencies(LLVMNode &node,
                             std::vector<LLVMNode *> &dependencies,
                             std::vector<LLVMNode *> &rev_data_dependencies);
  void apexDgGetNodeOrDie(const APEXDependencyGraph &apex_dg,
                          const Instruction *const I, APEXDependencyNode &node);
  void apexDgComputeFunctionDependencyBlocks(const Module &M);
  void apexDgPrintFunctionDependencyBlocks();
  void apexDgConstructBlocksFunctionsCallgraph();
  void apexDgPrintBlocksFunctionsCallgraph();

  // Module utilities.
  void moduleParseCmdLineArgsOrDie();
  void moduleFindTargetInstructionsOrDie(Module &M, const std::string &file,
                                         const std::string &line);
  void moduleInjectExitExtract(Module &M);
  void removeUnneededStuff(Module &M);
  void stripAllDebugSymbols(Module &M);
};

/// Registering our own pass, so it can be ran via opt.
char APEXPass::ID = 0;
static RegisterPass<APEXPass> X("apex", "Active code Path EXtractor.",
                                false /* Only looks at CFG */,
                                false /* Analysis Pass */);