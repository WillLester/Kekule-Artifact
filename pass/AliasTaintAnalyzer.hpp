#pragma once

#include "TaintAnalyzer.hpp"

using namespace llvm;

// A taint analyzer that doesn't track the fields
class AliasTaintAnalyzer : public TaintAnalyzer {
public:
	AliasTaintAnalyzer(Function* func, Value* tainted) : TaintAnalyzer(func, tainted) {};
	AliasTaintAnalyzer(Function* func, Value* tainted, CallArgSet visited) : TaintAnalyzer(func, tainted, visited) {};
	AliasTaintAnalyzer(Function* func, std::unordered_set<Value*> tainted) : TaintAnalyzer(func, tainted) {};
	AliasTaintAnalyzer(Function* func, std::unordered_set<Value*> tainted, CallArgSet visited)
		: TaintAnalyzer(func, tainted, visited) {};
private:
	bool visitCallBase(CallBase* ins);
	bool visitCallInst(CallInst* ins) override;
	bool visitExtractElementInst(ExtractElementInst* ins) override;
	bool visitExtractValueInst(ExtractValueInst* ins) override;
	bool visitGetElementPtrInst(GetElementPtrInst* ins) override;
	bool visitInsertElementInst(InsertElementInst* ins) override;
	bool visitInsertValueInst(InsertValueInst* ins) override;
	bool visitInvokeInst(InvokeInst* ins) override;
	bool visitSelectInst(SelectInst* ins) override;
	bool visitStoreInst(StoreInst* ins) override;
};
