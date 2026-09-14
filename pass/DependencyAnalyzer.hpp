#include <vector>
#include <functional>
#include <unordered_map>
#include "TypeAnalyzer.hpp"
#include "RegBehaviorAnalyzer.hpp"

using namespace llvm;

struct RegBehaviorVar {
	unsigned regBehaviorId;
	Var* var;
	RegBehaviorVar(unsigned regBehaviorId, Var* var) {
		this->regBehaviorId = regBehaviorId;
		this->var = var;
	}
	unsigned getVarId() const {
		return var->getId();
	}
	bool possibleAlias(const RegBehaviorVar* var) const {
		return this->var->possibleAlias(var->var);
	}
	bool isVarRead() const {
		return this->var->isVarRead();
	}
};

struct HashRegBehaviorVar {
	size_t operator() (const RegBehaviorVar* var) const {
		unsigned regId = var->regBehaviorId;
		unsigned id = var->var->getId();
		bool isRead = var->var->isVarRead();
		size_t hashValue = 17;
		hashValue = hashValue * 31 + std::hash<unsigned>()(regId);
		hashValue = hashValue * 31 + std::hash<unsigned>()(id);
		hashValue = hashValue * 31 + std::hash<bool>()(isRead);
		return hashValue;
	}
};

struct EqualRegBehaviorVar {
	bool operator() (const RegBehaviorVar* var1, const RegBehaviorVar* var2) const {
		return var1->regBehaviorId == var2->regBehaviorId && var1->getVarId() == var2->getVarId()
			   && var1->isVarRead() == var2->isVarRead();
	}
};


typedef std::unordered_set<RegBehaviorVar*, HashRegBehaviorVar, EqualRegBehaviorVar> RegBehaviorVarSet;
typedef std::unordered_map<RegBehaviorVar*, RegBehaviorVarSet, HashRegBehaviorVar,
			               EqualRegBehaviorVar> DependencyMap;

class DependencyAnalyzer {
public:
	DependencyAnalyzer(std::vector<RegBehavior*> behaviors) {
		this->behaviors = behaviors;
	}
	void analyze();
	void printToFile(std::string path);
private:
	std::vector<RegBehavior*> behaviors;
	DependencyMap depend;
	void analyzeWriteDep();
	void analyzeAssignDep();
	RegBehaviorVarSet transformVarMapToRegVarSet(unsigned regId,
									const std::unordered_map<BasicBlock*, std::vector<Var*>>& accessedVars);
};
