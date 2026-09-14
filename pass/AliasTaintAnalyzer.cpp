#include "AliasTaintAnalyzer.hpp"

using namespace llvm;

bool
AliasTaintAnalyzer::visitCallBase(CallBase* ins) {
	// Currently make the taint analysis intra-procedural.
	return false;
}

bool
AliasTaintAnalyzer::visitCallInst(CallInst* ins) {
	return visitCallBase(ins);
}

bool
AliasTaintAnalyzer::visitExtractElementInst(ExtractElementInst* ins) {
	return false;
}

bool
AliasTaintAnalyzer::visitExtractValueInst(ExtractValueInst* ins) {
	return false;
}

bool
AliasTaintAnalyzer::visitGetElementPtrInst(GetElementPtrInst* ins) {
	return false;
}

bool
AliasTaintAnalyzer::visitInsertElementInst(InsertElementInst* ins) {
	return false;
}

bool
AliasTaintAnalyzer::visitInsertValueInst(InsertValueInst* ins) {
	return false;
}

bool
AliasTaintAnalyzer::visitInvokeInst(InvokeInst* ins) {
	return visitCallBase(ins);
}

bool
AliasTaintAnalyzer::visitSelectInst(SelectInst* ins) {
	return false;
}

bool
AliasTaintAnalyzer::visitStoreInst(StoreInst* ins) {
	bool changed = false;
	if (isTainted(ins->getValueOperand())) {
		changed = setTainted(ins->getPointerOperand());
		if (setTainted(ins)) {
			return true;
		} else {
			return changed;
		}
	}
	return changed;
}
