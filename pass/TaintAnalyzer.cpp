#include "TaintAnalyzer.hpp"
#include <queue>


TaintAnalyzer::TaintAnalyzer(Function* func, Value* tainted) {
	this->starter = func;
	this->retTainted = false;
	this->taintFlags[tainted] = true;
	if (isa<Argument>(tainted)) {
		Argument* arg = cast<Argument>(tainted);
		std::pair<Function*, unsigned> callArg = std::make_pair(arg->getParent(), arg->getArgNo());
		visitedInterPro.insert(callArg);
	}
}

TaintAnalyzer::TaintAnalyzer(Function* func, Value* tainted, CallArgSet visited) : TaintAnalyzer(func, tainted) {
	visitedInterPro.insert(visited.begin(), visited.end());
}

TaintAnalyzer::TaintAnalyzer(Function* func, std::unordered_set<Value*> tainted) {
	this->starter = func;
	this->retTainted = false;
	for (Value* v : tainted) {
		this->taintFlags[v] = true;
		if (isa<Argument>(v)) {
			Argument* arg = cast<Argument>(v);
			std::pair<Function*, unsigned> callArg = std::make_pair(arg->getParent(), arg->getArgNo());
			visitedInterPro.insert(callArg);
		}
	}
}

TaintAnalyzer::TaintAnalyzer(Function* func, std::unordered_set<Value*> tainted, CallArgSet visited) : TaintAnalyzer(func, tainted) {
	visitedInterPro.insert(visited.begin(), visited.end());
}

void
TaintAnalyzer::analyze() {
	std::queue<BasicBlock*> visitQueue;
	// Push the entry block into the queue.
	visitQueue.push(&(starter->getEntryBlock()));
	// Visit the basic blocks until no update
	std::unordered_set<BasicBlock*> visited;
	while (!visitQueue.empty()) {
		BasicBlock* BB = visitQueue.front();
		visitQueue.pop();
		if (visitBB(BB) || visited.count(BB) == 0) {
			visited.insert(BB);
			Instruction* term = BB->getTerminator();
			for (unsigned i = 0; i < term->getNumSuccessors(); ++i) {
				visitQueue.push(term->getSuccessor(i));
			}
		}
	}
}

bool
TaintAnalyzer::isRetTainted() {
	return retTainted;
}

std::unordered_set<Value*>
TaintAnalyzer::getTaintedValues() {
	std::unordered_set<Value*> tainted;
	for (auto& itr : taintFlags) {
		if (itr.second) {
			tainted.insert(itr.first);
		}
	}
	return tainted;
}

bool
TaintAnalyzer::visitBB(BasicBlock* BB) {
	bool hasUpdate = false;
	for (BasicBlock::iterator i = BB->begin(); i != BB->end(); ++i) {
		if (visitInstruction(&(*i))) {
			hasUpdate = true;
		}
	}
	return hasUpdate;
}

bool
TaintAnalyzer::visitInstruction(Instruction* ins) {
	bool isTainted = false;
	if (taintFlags.count(ins) > 0) {
		isTainted = taintFlags[ins];
	}
	if (isTainted) {
		return false;
	}
	if (isa<AllocaInst>(ins)) {
		isTainted = visitAllocaInst(cast<AllocaInst>(ins));
	} else if (isa<AtomicRMWInst>(ins)) {
		isTainted = visitAtomicRMWInst(cast<AtomicRMWInst>(ins));
	} else if (isa<BinaryOperator>(ins)) {
		isTainted = visitBinaryOperator(cast<BinaryOperator>(ins));
	} else if (isa<BranchInst>(ins)) {
		isTainted = visitBranchInst(cast<BranchInst>(ins));
	} else if (isa<CallInst>(ins)) {
		isTainted = visitCallInst(cast<CallInst>(ins));
	} else if (isa<CmpInst>(ins)) {
		isTainted = visitCmpInst(cast<CmpInst>(ins));
	} else if (isa<ExtractElementInst>(ins)) {
		isTainted = visitExtractElementInst(cast<ExtractElementInst>(ins));
	} else if (isa<ExtractValueInst>(ins)) {
		isTainted = visitExtractValueInst(cast<ExtractValueInst>(ins));
	} else if (isa<FenceInst>(ins)) {
		isTainted = visitFenceInst(cast<FenceInst>(ins));
	} else if (isa<FreezeInst>(ins)) {
		isTainted = visitFreezeInst(cast<FreezeInst>(ins));
	} else if (isa<GetElementPtrInst>(ins)) {
		isTainted = visitGetElementPtrInst(cast<GetElementPtrInst>(ins));
	} else if (isa<InsertElementInst>(ins)) {
		isTainted = visitInsertElementInst(cast<InsertElementInst>(ins));
	} else if (isa<InsertValueInst>(ins)) {
		isTainted = visitInsertValueInst(cast<InsertValueInst>(ins));
	} else if (isa<InvokeInst>(ins)) {
		isTainted = visitInvokeInst(cast<InvokeInst>(ins));
	} else if (isa<LandingPadInst>(ins)) {
		isTainted = visitLandingPadInst(cast<LandingPadInst>(ins));
	} else if (isa<PHINode>(ins)) {
		isTainted = visitPHINode(cast<PHINode>(ins));
	} else if (isa<ReturnInst>(ins)) {
		isTainted = visitReturnInst(cast<ReturnInst>(ins));
	} else if (isa<ShuffleVectorInst>(ins)) {
		isTainted = visitShuffleVectorInst(cast<ShuffleVectorInst>(ins));
	} else if (isa<StoreInst>(ins)) {
		isTainted = visitStoreInst(cast<StoreInst>(ins));
	} else if (isa<LoadInst>(ins)) {
		isTainted = visitLoadInst(cast<LoadInst>(ins));
	} else if (isa<SelectInst>(ins)) {
		isTainted = visitSelectInst(cast<SelectInst>(ins));
	} else if (isa<SwitchInst>(ins)) {
		isTainted = visitSwitchInst(cast<SwitchInst>(ins));
	} else if (isa<CastInst>(ins)) {
		isTainted = visitCastInst(cast<CastInst>(ins));
	} else if (isa<UnreachableInst>(ins)) {
		isTainted = visitUnreachableInst(cast<UnreachableInst>(ins));
	} else {
		ins->print(errs());
		assert(false && "not valid instruction!");
	}
	return isTainted;
}

bool
TaintAnalyzer::isTainted(Value* val) {
	return taintFlags.count(val) > 0 && taintFlags[val] == true;
}

bool
TaintAnalyzer::isOpTainted(Instruction* ins) {
	for (unsigned i = 0; i < ins->getNumOperands(); ++i) {
		if (isTainted(ins->getOperand(i))) {
			return true;
		}
	}
	return false;
}

bool
TaintAnalyzer::setTainted(Value* val) {
	if (isTainted(val)) {
		return false;
	}
	taintFlags[val] = true;
	return true;
}

bool
TaintAnalyzer::visitAllocaInst(AllocaInst* ins) {
	return false;
}

bool
TaintAnalyzer::visitAtomicRMWInst(AtomicRMWInst* ins) {
	if (isTainted(ins->getPointerOperand())) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitBinaryOperator(BinaryOperator* ins) {
	if (isOpTainted(ins)) {
		return setTainted(ins);
	} else {
		return false;
	}
}

bool
TaintAnalyzer::visitBranchInst(BranchInst* ins) {
	return false;
}

bool
TaintAnalyzer::visitCallBase(CallBase* ins) {
	// Inter-procedural
	if (!ins->isIndirectCall() && !ins->isInlineAsm()) {
		Function* callee = ins->getCalledFunction();
		if (callee == NULL) {
			ins->print(errs());
		}
		if (!callee->isIntrinsic() && !callee->isDeclaration() && !callee->isVarArg()) {
			bool isCalleeRetTainted = false;
			for (unsigned i = 0; i < ins->arg_size(); ++i) {
				Value* arg = ins->getArgOperand(i);
				std::pair<Function*, unsigned> callArg = std::make_pair(callee, i);
				if (isTainted(arg) && visitedInterPro.count(callArg) == 0) {
					visitedInterPro.insert(callArg);
					TaintAnalyzer analyzer(callee, callee->getArg(i), visitedInterPro);
					analyzer.analyze();
					std::unordered_set<Value*> tainted = analyzer.getTaintedValues();
					for (Value* taintedVal : tainted) {
						setTainted(taintedVal);
					}
					if (analyzer.isRetTainted()) {
						isCalleeRetTainted = true;
					}
				}
			}
			// Only if the return value is tainted, this instruction is tainted.
			if (isCalleeRetTainted) {
				return setTainted(ins);
			}
		} else if (callee->isIntrinsic()) {
			if (callee->getName() == "llvm.fshl.i64") {
				if (isTainted(ins->getArgOperand(0)) || isTainted(ins->getArgOperand(1))) {
					return setTainted(ins);
				}
			}
		}
	}
	return false;
}

bool
TaintAnalyzer::visitCallInst(CallInst* ins) {
	return visitCallBase(ins);
}

bool
TaintAnalyzer::visitInvokeInst(InvokeInst* ins) {
	return visitCallBase(ins);
}

bool
TaintAnalyzer::visitCmpInst(CmpInst* ins) {
	return false;
}

bool
TaintAnalyzer::visitExtractElementInst(ExtractElementInst* ins) {
	if (isTainted(ins->getVectorOperand())) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitExtractValueInst(ExtractValueInst* ins) {
	if (isTainted(ins->getAggregateOperand())) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitFenceInst(FenceInst* ins) {
	return false;
}

bool
TaintAnalyzer::visitFreezeInst(FreezeInst* ins) {
	if (isTainted(ins->getOperand(0))) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitGetElementPtrInst(GetElementPtrInst* ins) {
	if (isTainted(ins->getPointerOperand())) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitInsertElementInst(InsertElementInst* ins) {
	if (isTainted(ins->getOperand(1))) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitInsertValueInst(InsertValueInst* ins) {
	if (isTainted(ins->getInsertedValueOperand())) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitLandingPadInst(LandingPadInst* ins) {
	return false;
}

bool
TaintAnalyzer::visitPHINode(PHINode* ins) {
	for (unsigned i = 0; i < ins->getNumIncomingValues(); ++i) {
		if (isTainted(ins->getIncomingValue(i))) {
			return setTainted(ins);
		}
	}
	return false;
}

bool
TaintAnalyzer::visitReturnInst(ReturnInst* ins) {
	if (isTainted(ins->getReturnValue())) {
		retTainted = true;
	}
	return false;
}

bool
TaintAnalyzer::visitShuffleVectorInst(ShuffleVectorInst* ins) {
	if (isTainted(ins->getOperand(0)) || isTainted(ins->getOperand(1))) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitStoreInst(StoreInst* ins) {
	if (isTainted(ins->getValueOperand())) {
		return setTainted(ins->getPointerOperand());
	}
	return false;
}

bool
TaintAnalyzer::visitLoadInst(LoadInst* ins) {
	if (isTainted(ins->getPointerOperand())) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitSelectInst(SelectInst* ins) {
	if (isTainted(ins->getTrueValue()) || isTainted(ins->getFalseValue())) {
		return true;
	}
	return false;
}

bool
TaintAnalyzer::visitSwitchInst(SwitchInst* ins) {
	return false;
}

bool
TaintAnalyzer::visitCastInst(CastInst* ins) {
	if (isOpTainted(ins)) {
		return setTainted(ins);
	}
	return false;
}

bool
TaintAnalyzer::visitUnreachableInst(UnreachableInst* ins) {
	return false;
}
