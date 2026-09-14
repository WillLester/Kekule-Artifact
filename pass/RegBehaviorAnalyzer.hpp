#pragma once
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Support/GenericDomTree.h"
#include "llvm/Support/CFGUpdate.h"
#include "TypeAnalyzer.hpp"
#include "QemuIndirectCallAnalyzer.hpp"
#include "VBoxIndirectCallAnalyzer.hpp"

using namespace llvm;

class Var {
public:
	Var(unsigned id, bool isRead, ValueType* type, Instruction* ins) {
		this->id = id;
		this->isRead = isRead;
		this->isMarked = false;
		this->type = type;
		this->ins = ins;
	}
	void print() {
		errs() << id << " " << isRead << " " << isMarked << "\n";
		type->print();
	}
	bool equal(const Var* var) const {
		return this->typeEqual(var->type) && this->isRead == var->isRead && this->isMarked == var->isMarked;
	}
	bool typeEqual(const ValueType* type) const {
		return (this->type->equal(type));
	}
	bool possibleAlias(const Var* var) const {
		return (this->type->possibleAlias(var->type));
	}
	unsigned getId() const {
		return id;
	}
	bool isVarRead() const {
		return isRead;
	}
	bool isVarMarked() const {
		return isMarked;
	}
	void setVarMarked(bool isMarked) {
		this->isMarked = isMarked;
	}
	Instruction* getIns() const {
		return ins;
	}

	struct VarEqual {
		bool operator()(const Var* a, const Var* b) const {
			return a->getId() == b->getId() && a->isVarRead() == b->isVarRead() && a->isVarMarked() == b->isVarMarked();
		}
	};

	struct VarHash {
		std::size_t operator()(const Var* v) const {
			size_t hash1 = std::hash<unsigned>()(v->id);
			size_t hash2 = std::hash<bool>()(v->isRead);
			return hash1 ^ (hash2 << 1);
		}
	};
private:
	unsigned id;
	bool isRead;
	bool isMarked;
	ValueType* type;
	Instruction* ins;
};

struct FuncArg {
	Function* sim;
	unsigned idx;
	FuncArg(Function* sim, unsigned idx) {
		this->sim = sim;
		this->idx = idx;
	}
};

struct HashBB {
	size_t operator()(const BasicBlock* BB) const {
		return std::hash<unsigned long>()((unsigned long) BB);
	}
};

typedef std::unordered_set<BasicBlock*, HashBB> BasicBlockSet;

struct HashPair {
	size_t operator()(const std::pair<unsigned, unsigned> pair) const {
		return std::hash<unsigned>()(pair.first) ^ std::hash<unsigned>()(pair.second);
	}
};

typedef std::unordered_map<std::pair<unsigned, unsigned>, BasicBlock*, HashPair> GroupMap;

class RegBehavior {
public:
	RegBehavior(unsigned id, Instruction* addrChecker, Instruction* begin, std::unordered_set<Instruction*> end,
			    std::unordered_map<BasicBlock*, std::vector<Var*>> accessedVars) {
		this->id = id;
		this->addrChecker = addrChecker;
		this->begin.push_back(begin);
		this->end = end;
		this->accessedVars = accessedVars;
	}
	RegBehavior(unsigned id, Instruction* addrChecker, std::vector<Instruction*> begin, std::unordered_set<Instruction*> end,
			    std::unordered_map<BasicBlock*, std::vector<Var*>> accessedVars) {
		this->id = id;
		this->addrChecker = addrChecker;
		this->begin = begin;
		this->end = end;
		this->accessedVars = accessedVars;
	}
	unsigned getId() {
		return id;
	}
	void addBegin(Instruction* ins) {
		this->begin.push_back(ins);
	}
	Instruction* getAddrChecker() {
		return addrChecker;
	}
	std::vector<Instruction*> getBegin() {
		return begin;
	}
	std::unordered_set<Instruction*> getEnd() {
		return end;
	}
	std::unordered_map<BasicBlock*, std::vector<Var*>> getAccessedVars() {
		return accessedVars;
	}
private:
	unsigned id;
	Instruction* addrChecker;
	std::vector<Instruction*> begin;
	std::unordered_set<Instruction*> end;
	std::unordered_map<BasicBlock*, std::vector<Var*>> accessedVars;
};

class RegBehaviorAnalyzer {
public:
	RegBehaviorAnalyzer(std::vector<FuncArg*> simFuncs, std::unordered_set<std::string> stateTypeNames, TypeAnalyzer* typeAnalyzer, std::string platform) {
		this->simFuncs = simFuncs;
		this->stateTypeNames = stateTypeNames;
		this->typeAnalyzer = typeAnalyzer;
		assert(!simFuncs.empty() && "No simulation functions\n");
		nextId = 0;
		nextVarId = 0;
		if (platform == "QEMU") {
			indirectCallAnalyzer = new QemuIndirectCallAnalyzer();
		} else {
			indirectCallAnalyzer = new VBoxIndirectCallAnalyzer();
		}
		std::vector<Function*> writeSims;
		for (FuncArg* simFunc : simFuncs) {
			writeSims.push_back(simFunc->sim);
		}
		indirectCallAnalyzer->analyze(writeSims);
	}
	std::vector<RegBehavior*> analyze();
	DominatorTree* getDominatorTree(Function* func);
	PostDominatorTree* getPostDominatorTree(Function* func);
	void printDebugLineAndCol(Instruction* ins) const;
private:
	unsigned nextId;
	unsigned nextVarId;
	std::vector<FuncArg*> simFuncs;
	std::unordered_set<std::string> stateTypeNames;
	std::unordered_set<Instruction*> addrCheckers;
	std::vector<RegBehavior*> behaviors;
	std::unordered_map<Function*, DominatorTree*> domTrees;
	std::unordered_map<Function*, PostDominatorTree*> postDomTrees;
	std::unordered_set<BasicBlock*> termedBBs;
	std::unordered_map<Function*, bool> funcHasAddrChecker;
	std::unordered_map<Function*, bool> funcChangeState;
	std::unordered_map<BasicBlock*, bool> bbChangeState;
	std::vector<Var*> vars;
	std::unordered_map<Function*, std::unordered_map<BasicBlock*, std::vector<Var*>>> funcAccessedVars;
	std::unordered_map<BasicBlock*, std::vector<Var*>> bbAccessedVars;
	TypeAnalyzer* typeAnalyzer;
	IndirectCallAnalyzer* indirectCallAnalyzer;
	unsigned assignNextId();
	unsigned assignNextVarId();
	Var* getVarByValueType(ValueType* type, bool isRead, Instruction* ins);
	void collectBBsInPath(BasicBlockSet& visited, BasicBlock* cur, BasicBlock* end, std::vector<BasicBlock*>& path,
						  BasicBlockSet& result);
	BasicBlockSet getPredBBs(BasicBlock* start, BasicBlock* end);
	void removeBBsFromSet(BasicBlockSet& set, BasicBlockSet& removed);
	void getBranchesOnAddr();
	bool changeStateBeforeMerging(BasicBlock* start, BasicBlock* groupPoint);
	std::vector<RegBehavior*> getRegBehaviorFromBranch(Instruction* addChecker, BasicBlock* start, BasicBlockSet& bbs,
													   std::unordered_set<Instruction*>& addrCheckerVisited,
													   BasicBlockSet& predBBs,
													   std::unordered_set<Function*>& funcVisited, bool isBBLimit);
	std::vector<RegBehavior*> checkAddrChecker(Instruction* addrChecker, std::unordered_set<Instruction*>& addrCheckerVisited,
											   BasicBlockSet& predBBs,
											   std::unordered_set<Function*>& funcVisited);
	void extractRegBehavior();
};
