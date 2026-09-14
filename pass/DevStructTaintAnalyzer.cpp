#include "DevStructTaintAnalyzer.hpp"
#include <regex>

using namespace llvm;

std::unordered_set<Value*>
DevStructTaintAnalyzer::getAliases() {
	return aliases;
}


bool
DevStructTaintAnalyzer::isAliased(Value* value) {
	return aliases.count(value) > 0;
}

bool
DevStructTaintAnalyzer::setAliased(Value* value) {
	if (isAliased(value)) {
		return false;
	}
	aliases.insert(value);
	return true;
}

bool
DevStructTaintAnalyzer::visitCallBase(CallBase* ins) {
	bool changed = false;
	bool argTainted = false;
	for (unsigned i = 0; i < ins->arg_size(); ++i) {
		Value* arg = ins->getArgOperand(i);
		if (isTainted(arg)) {
			argTainted = true;
			break;
		}
	}
	if (argTainted) {
		Function* callee = ins->getCalledFunction();
		if (callee == nullptr) {
			return changed;
		}
		// The object is casted to the state
		if (callee->hasName() && callee->getName().equals("object_dynamic_cast_assert")) {
			std::vector<std::string> noStateSuffixes = {"CLASS","BUS","BUS_DEVICE","CONSOLE"};
			std::vector<std::string> noStateFunctions = {"DEVICE","USB_DEVICE","BUS","PCI_BUS","SCSI_DEVICE",
														 "SYSBUS_ESP","SD_CARD"};
			Value* transform = ins->getArgOperand(4);
			bool isStateCast = true;
			StringRef transName = transform->getName();
			std::regex pattern("(\\.[0-9]+)$");
			std::string res = std::regex_replace(transName.str(), pattern, "");
			transName = StringRef(res);
			if (setTainted(ins)) {
				changed = true;
			}
			for (std::string suffix : noStateSuffixes) {
				if (transName.endswith(suffix)) {
					isStateCast = false;
					break;
				}
			}
			for (std::string excluded : noStateFunctions) {
				if (transName.endswith("." + excluded)) {
					isStateCast = false;
					break;
				}
			}
			if (!isStateCast) {
				return changed;
			}
			hasObjectCast = true;
			errs() << "cast " << transform->getName() << "\n";
			ins->print(errs());
			errs() << "\n";
			if (setAliased(ins)) {
				changed = true;
			}
		} else {
			if (!ins->isIndirectCall() && !ins->isInlineAsm()) {
				if (!callee->isIntrinsic() && !callee->isDeclaration() && !callee->isVarArg()) {
					bool isCalleeRetTainted = false;
					bool isCalleeRetAliased = false;
					for (unsigned i = 0; i < ins->arg_size(); ++i) {
						Value* arg = ins->getArgOperand(i);
						std::pair<Function*, unsigned> callArg = std::make_pair(callee, i);
						if (isTainted(arg) && visitedInterPro.count(callArg) == 0) {
							visitedInterPro.insert(callArg);
							DevStructTaintAnalyzer analyzer(callee, callee->getArg(i), visitedInterPro);
							analyzer.analyze();
							std::unordered_set<Value*> tainted = analyzer.getTaintedValues();
							for (Value* taintedVal : tainted) {
								setTainted(taintedVal);
							}
							std::unordered_set<Value*> interAliases = analyzer.getAliases();
							for (Value* alias : interAliases) {
								setAliased(alias);
							}
							if (analyzer.isRetTainted()) {
								isCalleeRetTainted = true;
							}
							if (analyzer.isRetAliased()) {
								isCalleeRetAliased = true;
							}
							if (analyzer.hasObjectCastCall()) {
								hasObjectCast = true;
							}
						}
					}
					// Only if the return value is tainted, this instruction is tainted.
					if (isCalleeRetTainted) {
						changed = setTainted(ins);
					}
					if (isCalleeRetAliased) {
						if (setAliased(ins)) {
							changed = true;
						}
					}
				}
			}
		}
	}
	return changed;
}

bool
DevStructTaintAnalyzer::visitBinaryOperator(BinaryOperator* ins) {
	return false;
}

bool
DevStructTaintAnalyzer::visitCallInst(CallInst* ins) {
	return visitCallBase(ins);
}

bool
DevStructTaintAnalyzer::visitExtractElementInst(ExtractElementInst* ins) {
	return false;
}

bool
DevStructTaintAnalyzer::visitExtractValueInst(ExtractValueInst* ins) {
	return false;
}

bool
DevStructTaintAnalyzer::visitGetElementPtrInst(GetElementPtrInst* ins) {
	return false;
}

bool
DevStructTaintAnalyzer::visitInsertElementInst(InsertElementInst* ins) {
	return false;
}

bool
DevStructTaintAnalyzer::visitInsertValueInst(InsertValueInst* ins) {
	return false;
}

bool
DevStructTaintAnalyzer::visitInvokeInst(InvokeInst* ins) {
	return visitCallBase(ins);
}

bool
DevStructTaintAnalyzer::visitLoadInst(LoadInst* ins) {
	if (isTainted(ins->getPointerOperand())) {
		bool taintChanged = setTainted(ins);
		bool aliasChanged = setAliased(ins);
		return taintChanged || aliasChanged;
	} else {
		return false;
	}
}

bool
DevStructTaintAnalyzer::visitReturnInst(ReturnInst* ins) {
	if (isTainted(ins->getReturnValue())) {
		retTainted = true;
	}
	if (isAliased(ins->getReturnValue())) {
		retAliased = true;
	}
	return false;
}

bool
DevStructTaintAnalyzer::visitSelectInst(SelectInst* ins) {
	return false;
}

