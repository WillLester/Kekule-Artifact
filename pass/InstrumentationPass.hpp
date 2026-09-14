#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include "RegBehaviorAnalyzer.hpp"
#include "AliasTaintAnalyzer.hpp"

using namespace llvm;

class InstrumentationPass : public PassInfoMixin<InstrumentationPass> {
public:
	PreservedAnalyses run(Module& M, ModuleAnalysisManager& AM);
private:
    Function* feedback;
	std::vector<Function*> writeSims;
	std::unordered_set<std::string> stateTypeNames;
	std::unordered_set<LoadInst*> markedLoads;
	RegBehaviorAnalyzer* regAnalyzer;
	void createFeedbackFunc(Module& M);
	bool isMarked(LoadInst* load);
	void markVars(std::unordered_map<BasicBlock*, std::vector<Var*>>& accesses);
	std::unordered_map<BasicBlock*, std::vector<Var*>> findInstrumentationPoints(std::unordered_map<BasicBlock*, std::vector<Var*>> accesses);
	void instrument(std::vector<RegBehavior*> behaviors, std::unordered_map<BasicBlock*, std::vector<Var*>> points);
	void cleanup(Module& M);
};
