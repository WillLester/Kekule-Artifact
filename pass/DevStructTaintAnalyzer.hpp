#pragma once

#include "TaintAnalyzer.hpp"

using namespace llvm;

// A taint analyzer that doesn't track the fields
class DevStructTaintAnalyzer : public TaintAnalyzer {
public:
	DevStructTaintAnalyzer(Function* func, Value* tainted) : TaintAnalyzer(func, tainted) {
		aliases.insert(tainted);
	};
	DevStructTaintAnalyzer(Function* func, Value* tainted, CallArgSet visited) : TaintAnalyzer(func, tainted, visited) {};
	DevStructTaintAnalyzer(Function* func, std::unordered_set<Value*> tainted) : TaintAnalyzer(func, tainted) {};
	DevStructTaintAnalyzer(Function* func, std::unordered_set<Value*> tainted, CallArgSet visited)
		: TaintAnalyzer(func, tainted, visited) {};
	std::unordered_set<Value*> getAliases();
	bool hasObjectCastCall() {
		return hasObjectCast;
	}
	bool isRetAliased() {
		return retAliased;
	}
private:
	std::unordered_set<Value*> aliases;
	bool hasObjectCast = false;
	bool retAliased = false;
	bool isAliased(Value* value);
	bool setAliased(Value* value);
	bool visitCallBase(CallBase* ins);
	bool visitBinaryOperator(BinaryOperator* ins) override;
	bool visitCallInst(CallInst* ins) override;
	bool visitExtractElementInst(ExtractElementInst* ins) override;
	bool visitExtractValueInst(ExtractValueInst* ins) override;
	bool visitGetElementPtrInst(GetElementPtrInst* ins) override;
	bool visitInsertElementInst(InsertElementInst* ins) override;
	bool visitInsertValueInst(InsertValueInst* ins) override;
	bool visitInvokeInst(InvokeInst* ins) override;
	bool visitLoadInst(LoadInst* ins) override;
	bool visitReturnInst(ReturnInst* ins) override;
	bool visitSelectInst(SelectInst* ins) override;
};
