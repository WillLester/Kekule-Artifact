#include <fstream>
#include "DependencyAnalyzer.hpp"
#include "AliasTaintAnalyzer.hpp"

static cl::opt<std::string> dependOutputPath("depend_output_path",
	cl::init(""),
	cl::desc("The path where the dependency should be output to."),
	cl::value_desc("The path where the dependency should be output to."));

void
DependencyAnalyzer::analyze() {
	analyzeWriteDep();
	analyzeAssignDep();
	printToFile(dependOutputPath);
}

void
DependencyAnalyzer::printToFile(std::string path) {
	std::ofstream output(path, std::ios::trunc);
	if (output.is_open()) {
		for (auto itr : depend) {
			RegBehaviorVar* cur = itr.first;
			RegBehaviorVarSet& targets = itr.second;
			output << cur->regBehaviorId << "." << cur->var->getId() << "." << cur->var->isVarRead() << "\n";
			for (RegBehaviorVar* target : targets) {
				output << target->regBehaviorId << "." << target->var->getId() << "." << target->var->isVarRead() << ";";
			}
			output << "\n";
		}
		output.close();
	}
}

/**
 *  The dependency between a write and a read, the read needs to read the value written by the write.
 */

void
DependencyAnalyzer::analyzeWriteDep() {
	// Collect all vars accessed in each behavior
	std::unordered_map<unsigned, RegBehaviorVarSet> accessedVars;
	for (RegBehavior* behavior : behaviors) {
		accessedVars[behavior->getId()] = transformVarMapToRegVarSet(behavior->getId(), behavior->getAccessedVars());
	}
	for (auto itr1 : accessedVars) {
		RegBehaviorVarSet accessed1 = itr1.second;
		for (auto itr2 : accessedVars) {
			RegBehaviorVarSet accessed2 = itr2.second;
			for (RegBehaviorVar* var1 : accessed1) {
				if (!var1->isVarRead()) {
					for (RegBehaviorVar* var2 : accessed2) {
						if (var2->isVarRead()) {
							if (var1->possibleAlias(var2)) {
								depend[var1].insert(var2);
							}
						}              		
					}                  		
				}                      		
			}                          		
		}
	}
}

/**
 *  The dependency between a read and a write, the write will read the value and then write based on that value.
 */

void
DependencyAnalyzer::analyzeAssignDep() {
	// The dependency should only exist within one register behavior.
	for (RegBehavior* behavior : behaviors) {
		std::unordered_map<BasicBlock*, std::vector<Var*>> accessedVars = behavior->getAccessedVars();
		std::unordered_map<Instruction*, Var*> insMap; // Mapping between the instruction and the variable
		for (auto itr : accessedVars) {
			std::vector<Var*> vars = itr.second;
			for (Var* var : vars) {
				insMap[var->getIns()] = var;
			}
		}
		// For each load, taint it, if it touches any store, construct a dep between them.
		for (auto itr : insMap) {
			Instruction* ins = itr.first;
			if (isa<LoadInst>(ins)) {
				std::unordered_set<Instruction*> affected;
				AliasTaintAnalyzer analyzer(ins->getFunction(), ins);
				analyzer.analyze();
				std::unordered_set<Value*> tainted = analyzer.getTaintedValues();
				for (Value* v : tainted) {
					if (isa<StoreInst>(v)) {
						StoreInst* store = cast<StoreInst>(v);
						if (insMap.count(store) > 0) {
							affected.insert(store);
						}
					}
				}
				Var* loadVar = insMap[ins];
				RegBehaviorVar* var1 = new RegBehaviorVar(behavior->getId(), loadVar);
				for (Instruction* store : affected) {
					Var* storeVar = insMap[store];
					depend[var1].insert(new RegBehaviorVar(behavior->getId(), storeVar));
				}
			}
		}
	}
}

RegBehaviorVarSet
DependencyAnalyzer::transformVarMapToRegVarSet(unsigned regId,
					const std::unordered_map<BasicBlock*, std::vector<Var*>>& accessedVars) {
	std::unordered_set<Var*> mergedVars;
	for (auto itr : accessedVars) {
		std::vector<Var*>& vars = itr.second;
		mergedVars.insert(vars.begin(), vars.end());
	}
	RegBehaviorVarSet res;
	for (Var* var : mergedVars) {
		res.insert(new RegBehaviorVar(regId, var));
	}
	return res;
}
