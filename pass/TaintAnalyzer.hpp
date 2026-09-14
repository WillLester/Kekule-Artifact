#pragma once

#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <functional>
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

class TaintAnalyzer {
public:
	struct hashFunctionArg {
		std::size_t operator() (const std::pair<Function*, unsigned>& callInstArg) const {
			std::size_t hashInst = std::hash<Function*>{}(callInstArg.first);
			std::size_t hashArg = std::hash<unsigned>{}(callInstArg.second);
			return hashInst ^ hashArg;
		}
	};
	typedef std::unordered_set<std::pair<Function*, unsigned>, hashFunctionArg> CallArgSet;
	std::unordered_set<Value*> getTaintedValues();
	TaintAnalyzer(Function* func, Value* tainted);
	TaintAnalyzer(Function* func, Value* tainted, CallArgSet visited);
	TaintAnalyzer(Function* func, std::unordered_set<Value*> tainted);
	TaintAnalyzer(Function* func, std::unordered_set<Value*> tainted, CallArgSet visited);
	void analyze();
	bool isRetTainted();
protected:
	Function* starter;
	bool retTainted;
	std::unordered_map<Value*, bool> taintFlags;
	CallArgSet visitedInterPro;
	bool isTainted(Value* val);
	bool isOpTainted(Instruction* ins);
	bool setTainted(Value* val);
	bool visitBB(BasicBlock* BB);
	bool visitInstruction(Instruction* ins);
	bool visitCallBase(CallBase* ins);
	virtual bool visitAllocaInst(AllocaInst* ins);
	virtual bool visitAtomicRMWInst(AtomicRMWInst* ins);
	virtual bool visitBinaryOperator(BinaryOperator* ins);
	virtual bool visitBranchInst(BranchInst* ins);
	virtual bool visitCallInst(CallInst* ins);
	virtual bool visitCmpInst(CmpInst* ins);
	virtual bool visitExtractElementInst(ExtractElementInst* ins);
	virtual bool visitExtractValueInst(ExtractValueInst* ins);
	virtual bool visitFenceInst(FenceInst* ins);
	virtual bool visitFreezeInst(FreezeInst* ins);
	virtual bool visitGetElementPtrInst(GetElementPtrInst* ins);
	virtual bool visitInsertElementInst(InsertElementInst* ins);
	virtual bool visitInsertValueInst(InsertValueInst* ins);
	virtual bool visitInvokeInst(InvokeInst* ins);
	virtual bool visitLandingPadInst(LandingPadInst* ins);
	virtual bool visitPHINode(PHINode* ins);
	virtual bool visitReturnInst(ReturnInst* ins);
	virtual bool visitShuffleVectorInst(ShuffleVectorInst* ins);
	virtual bool visitStoreInst(StoreInst* ins);
	virtual bool visitLoadInst(LoadInst* ins);
	virtual bool visitSelectInst(SelectInst* ins);
	virtual bool visitSwitchInst(SwitchInst* ins);
	virtual bool visitCastInst(CastInst* ins);
	virtual bool visitUnreachableInst(UnreachableInst* ins);
};
