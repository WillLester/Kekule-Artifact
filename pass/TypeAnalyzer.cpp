#include "TypeAnalyzer.hpp"
#include "Utility.hpp"
#include "llvm/BinaryFormat/Dwarf.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include <queue>
#include <stack>
#include <iostream>
#include <regex>

using namespace llvm;

TypeAnalyzer::TypeAnalyzer(Module* M) {
	this->M = M;
}

void
TypeAnalyzer::analyze() {
    collectTypeInfo();
	propagateTypeInfo();
}

std::string
TypeAnalyzer::getValueType(Value* value) {
	if (types.count(value) == 0) {
		return "undefined";
	}
	return getType(value)->getType();
}

ValueType*
TypeAnalyzer::getValueVType(Value* value) {
	ValueType* type = types[value];
	assert(type != NULL && "no type for this var\n");
	return type;
}

std::unordered_set<Value*>
TypeAnalyzer::getTypeIntersection(std::unordered_set<Value*> set1, std::unordered_set<Value*> set2) {
	std::unordered_set<Value*> res;
	for (Value* v1 : set1) {
		ValueType* type1 = getType(v1);
		for (Value* v2 : set2) {
			ValueType* type2 = getType(v2);
			if (type1->possibleAlias(type2)) {
				res.insert(v1);
			}
		}
	}
	return res;
}

bool
TypeAnalyzer::fieldOfType(Value* v, std::string type) {
	ValueType* vType = getType(v);
	if (vType == NULL) {
		return false;
	}
	return vType->fieldOfType(type);
}

bool
TypeAnalyzer::fieldOfTypes(Value* v, std::unordered_set<std::string> types) {
	for (std::string type : types) {
		if (fieldOfType(v, type)) {
			return true;
		}
	}
	return false;
}

void
TypeAnalyzer::collectTypeInfo() {
	for (Module::iterator i = M->begin(); i != M->end(); ++i) {
		setFuncSignatureTypes(&(*i));
		for (Function::iterator j = i->begin(); j != i->end(); ++j) {
			for (BasicBlock::iterator k = j->begin(); k != j->end(); ++k) {
				Instruction* ins = &(*k);
				if (isa<GetElementPtrInst>(ins)) {
					GetElementPtrInst* gep = cast<GetElementPtrInst>(ins);
					Type* sourceTy = gep->getSourceElementType();
					ValueType* pointerType = getValueTypeByType(sourceTy);
					if (pointerType != NULL) {
						Value* pointer = gep->getPointerOperand();
						if (pointerType->isArray()) {
							assignTypeToValue(pointer, pointerType);
						} else {
							ValueType* ptrPointerType = pointerType->toPointer();
							assignTypeToValue(pointer, ptrPointerType);
							delete pointerType;
							pointerType = ptrPointerType;
						}
						Type* resultTy = gep->getResultElementType();
						ValueType* valueType = getValueTypeByType(resultTy, pointerType, extractIndices(gep));
						if (valueType != NULL) {
							if (valueType->isArray()) {
								assignTypeToValue(gep, valueType);
							} else {
								assignTypeToValue(gep, valueType->toPointer());
								delete valueType;
							}
						}
					}
				} else if (isa<ReturnInst>(ins)) {
					// Assign the function's return value type to the return value
					ReturnInst* ret = cast<ReturnInst>(ins);
					Value* retVal = ret->getReturnValue();
					if (retVal != NULL) {
						assignTypeToValue(retVal, getReturnType(&(*i)));
					}
				} else if (isa<DbgDeclareInst>(ins)) {
					DbgDeclareInst* dbg = cast<DbgDeclareInst>(ins);
					DILocalVariable* var = dbg->getVariable();
					if (var != NULL) {
						DIType* type = var->getType();
						Value* val = dbg->getAddress();
						ValueType* valueType = new ValueType(DITypeToString(type));
						if (valueType->isStruct()) {
							if (val->getType()->isPointerTy()) {
								assignTypeToValue(val, valueType->toPointer());
								delete valueType;
							}
						} else {
							assignTypeToValue(val, valueType);
						}
					}
				} else if (isa<DbgValueInst>(ins)) {
					DbgValueInst* dbg = cast<DbgValueInst>(ins);
					DILocalVariable* var = dbg->getVariable();
					if (var != NULL) {
						DIType* type = var->getType();
						Value* val = dbg->getValue();
						DIExpression* expression = dbg->getExpression();
						if (expression->isFragment()) {
							continue;
						}
						ValueType* valueType = new ValueType(DITypeToString(type));
						if (expression->startsWithDeref()) {
							ValueType* pointerType = valueType->toPointer();
							delete valueType;
							valueType = pointerType;
						}
						assignTypeToValue(val, valueType);
					}
				}
			}
		}
	}
}

void
TypeAnalyzer::propagateTypeInfo() {
	for (Module::iterator i = M->begin(); i != M->end(); ++i) {
		Function* func = &(*i);
		if (visitedFuncs.count(func) == 0) {
			visitedFuncs.insert(func);
			visitFunction(func);
		}
	}
}

void
TypeAnalyzer::setFuncSignatureTypes(Function* func) {
	DISubprogram* subprogram = func->getSubprogram();
	if (subprogram == NULL) {
		return;
	}
	DISubroutineType* type = subprogram->getType();
	DITypeRefArray typeArray = type->getTypeArray();
	if (typeArray.size() != func->arg_size() + 1) {
		errs() << "number of parameters not consistent\n";
		return;
	}
	for (unsigned i = 0; i < typeArray.size(); ++i) {
		DIType* argType = typeArray[i];
		std::string typeName = DITypeToString(argType);
		if (i == 0) {
			// Set return value
			assignReturnTypeToFunc(func, new ValueType(typeName));
		} else {
			if (argType == NULL) {
				break;
			}
			Argument* arg = func->getArg(i - 1);
			assignTypeToValue(arg, new ValueType(typeName));
		}
	}
}

void
TypeAnalyzer::visitFunction(Function* func) {
	if (func->isIntrinsic() || func->isDeclaration()) {
		return;
	}
	// The analysis becomes inter-procedural
	std::queue<BasicBlock*> worklist;
	BasicBlock* entry = &(func->getEntryBlock());
	worklist.push(entry);
	std::unordered_set<BasicBlock*> visited;
	while (!worklist.empty()) {
		BasicBlock* BB = worklist.front();
		worklist.pop();
		if (visitBB(BB) || visited.count(BB) == 0) {
			visited.insert(BB);
			Instruction* term = BB->getTerminator();
			for (unsigned i = 0; i < term->getNumSuccessors(); ++i) {
				worklist.push(term->getSuccessor(i));
			}
		}
	}
}

// Merge the indices of type to the current val when the val has a type
bool
TypeAnalyzer::updateIndices(Value* val, ValueType* type) {
	if (val == NULL || type == NULL) {
		return false;
	}
	if (types.count(val) == 0) {
		return false;
	} else {
		ValueType* valueType = getType(val);
		return valueType->mergeIndices(type);
	}
}

// Merge the indices of type to the current val when the val has a type
// Avoid type loop
bool
TypeAnalyzer::updateIndicesNoLoop(Value* val, ValueType* type) {
	if (val == NULL || type == NULL) {
		return false;
	}
	if (types.count(val) == 0) {
		return false;
	} else {
		ValueType* valueType = getType(val);
		return valueType->mergeIndicesNoLoop(type);
	}
}

bool
TypeAnalyzer::updateTypeAndIndices(Value* val, ValueType* type) {
	if (val == NULL || type == NULL) {
		return false;
	}
	if (types.count(val) == 0) {
		return assignTypeToValue(val, type);
	} else {
		return updateIndices(val, type);
	}
}

bool
TypeAnalyzer::assignTypeToValue(Value* val, ValueType* type) {
	if (val == NULL || type == NULL) {
		return false;
	}
	if (isa<ConstantData>(val)) {
		return false;
	}
	if (types.count(val) == 0) {
		types[val] = type;
		return true;
	} else {
		ValueType* valueType = getType(val);
		return valueType->mergeValueType(type);
	}
}

bool
TypeAnalyzer::assignTypeOnlyToValue(Value* val, ValueType* type) {
	if (val == NULL || type == NULL) {
		return false;
	}
	if (isa<ConstantData>(val)) {
		return false;
	}
	if (types.count(val) == 0) {
		types[val] = new ValueType(type->getType());
		return true;
	} else {
		ValueType* valueType = getType(val);
		return valueType->mergeValueTypeOnly(type);
	}
}

bool
TypeAnalyzer::assignReturnTypeToFunc(Function* func, ValueType* type) {
	returnTypes[func] = type;
	return true;
}

std::string
TypeAnalyzer::typeToString(Type* type) {
	if (type->isStructTy()) {
		return type->getStructName().split('.').second.str();
	} else if (type->isIntegerTy()) {
		IntegerType* intTy = cast<IntegerType>(type);
		switch (intTy->getBitWidth()) {
		case 1:
			return BOOL_TY;
		case 8:
			return UNSIGNED_CHAR_TY;
		case 16:
			return UNSIGNED_SHORT_TY;
		case 24:
			return UNSIGNED_INT24_TY;
		case 32:
			return UNSIGNED_INTEGER_TY;
		case 56:
			return UNSIGNED_INT56_TY;
		case 64:
			return UNSIGNED_LONG_TY;
		case 96:
			return UNSIGNED_INT96_TY;
		case 128:
			return UNSIGNED_INT128_TY;
		default:
			errs() << "Int length " << intTy->getBitWidth() << "\n";
			assert(false && "Not normal integer\n");
		}
	} else if (type->isVoidTy()) {
		return VOID_TY;
	} else if (type->isFloatTy()) {
		return FLOAT_TY;
	} else if (type->isDoubleTy()) {
		return DOUBLE_TY;
	} else if (type->isPointerTy()) {
		return PTR_TY;
	} else if (type->isArrayTy()) {
		std::string elementTy = typeToString(type->getArrayElementType());
		if (elementTy != PTR_TY && elementTy != EMPTY_TY) {
			return elementTy + "[" + std::to_string(type->getArrayNumElements()) + "]";
		}
	} else if (type->isVectorTy()) {
		VectorType* vectorTy = cast<VectorType>(type);
		std::string elementTy = typeToString(vectorTy->getElementType());
		if (elementTy != PTR_TY && elementTy != EMPTY_TY) {
			return elementTy + "<" + std::to_string(vectorTy->getElementCount().getFixedValue()) + ">";
		}
	} else if (type->isMetadataTy()) {
		return EMPTY_TY;
	} else {
		type->print(errs());
		assert(false && "Unknown type to string\n");
	}
	return EMPTY_TY;
}

ValueType*
TypeAnalyzer::getValueTypeByType(Type* type) {
	std::string typeName = typeToString(type);
	if (typeName == EMPTY_TY) {
		return NULL;
	}
	return new ValueType(typeName);
}

ValueType*
TypeAnalyzer::getValueTypeByType(Type* type, ValueType* parent, std::vector<Value*> indices) {
	std::string typeName = typeToString(type);
	if (typeName == PTR_TY || typeName == EMPTY_TY) {
		return NULL;
	}
	return new ValueType(typeName, parent, indices);
}

std::string
TypeAnalyzer::DITypeToString(DIType* type) {
	unsigned layer = 0;
	bool isUnion = false;
	unsigned unionSize;
	std::stack<unsigned long> arraySizes;
	std::string typedefName = "";
	while (type != NULL && isa<DICompositeType>(type)) {
		if (type->getTag() == dwarf::DW_TAG_enumeration_type) {
			DICompositeType* composite = cast<DICompositeType>(type);
			type = composite->getBaseType();
		} else if (type->getTag() == dwarf::DW_TAG_array_type) {
			DICompositeType* composite = cast<DICompositeType>(type);
			type = composite->getBaseType();
			DINode* node = composite->getElements()[0];
			assert(isa<DISubrange>(node) && "elements are not subrange\n");
			DISubrange* range = cast<DISubrange>(node);
			ConstantInt* elementNum = range->getCount().dyn_cast<ConstantInt*>();
			if (elementNum) {
				arraySizes.push(elementNum->getZExtValue());
			} else {
				arraySizes.push(0);
			}
		} else if (type->getTag() == dwarf::DW_TAG_union_type) {
			isUnion = true;
			unionSize = type->getSizeInBits();
			break;
		} else {
			break;
		}
	}
	while (type != NULL && isa<DIDerivedType>(type)) {
		if (type->getTag() == dwarf::DW_TAG_pointer_type) {
			++layer;
		} else if (type->getTag() == dwarf::DW_TAG_typedef) {
			typedefName = type->getName().str();
		} else if (type->getTag() == dwarf::DW_TAG_const_type) {
		} else if (type->getTag() == dwarf::DW_TAG_restrict_type) {
		} else if (type->getTag() == dwarf::DW_TAG_volatile_type) {
		} else {
			type->print(errs());
			assert(false && "Unprocessed derived type\n");
		}
		DIDerivedType* derived = cast<DIDerivedType>(type);
		type = derived->getBaseType();
	}
	std::string typeName;
	if (type == NULL) {
		typeName = VOID_TY;
	} else if (isa<DISubroutineType>(type)) {
		typeName = "";
		DISubroutineType* subRouTy = cast<DISubroutineType>(type);
		DITypeRefArray typeArray = subRouTy->getTypeArray();
		for (unsigned i = 0; i < typeArray.size(); ++i) {
			DIType* argType = typeArray[i];
			std::string argTypeName = DITypeToString(argType);
			typeName = typeName + argTypeName;
			if (i < typeArray.size() - 1) {
				typeName = typeName + " ";
			}
		}
	} else {
		typeName = type->getName().str();
	}
	if (typeName == "_Bool") {
		typeName = BOOL_TY;
	}
	if (typeName == "" && typedefName != "") {
		typeName = typedefName;
	}
	if (typeName == "") {
		errs() << "empty type\n";
		type->print(errs());
		errs() << "\n";
	}
	while (!arraySizes.empty()) {
		typeName = typeName + '[';
		typeName = typeName + std::to_string(arraySizes.top());
		typeName = typeName + ']';
		arraySizes.pop();
	}
	for (unsigned i = 0; i < layer; ++i) {
		typeName = typeName + '*';
	}
	if (isUnion)
		typeName = typeName + '{' + std::to_string(unionSize) + '}';
	return typeName;
}

std::vector<Value*>
TypeAnalyzer::extractIndices(GetElementPtrInst* gep) {
	std::vector<Value*> res;
	Type* sourceTy = gep->getSourceElementType();
	for (GetElementPtrInst::op_iterator i = gep->idx_begin(); i != gep->idx_end(); ++i) {
		if (sourceTy->isStructTy() && i == gep->idx_begin()) {
			// Struct GEP starts from 0, ignore it
			continue;
		}
	}
	return res;
}

inline ValueType*
TypeAnalyzer::getType(Value* val) {
	if (types.count(val) == 0) {
		return getValueTypeByType(val->getType());
	} else {
		return types[val];
	}
}

inline ValueType*
TypeAnalyzer::getReturnType(Function* func) {
	return returnTypes[func];
}

bool
TypeAnalyzer::visitBB(BasicBlock* BB) {
	bool hasUpdate = false;
	for (BasicBlock::iterator i = BB->begin(); i != BB->end(); ++i) {
		if (visitInstruction(&(*i))) {
			hasUpdate = true;
		}
	}
	return hasUpdate;
}

bool
TypeAnalyzer::visitInstruction(Instruction* ins) {
	bool changedType = false;
	if (isa<AllocaInst>(ins)) {
		changedType = visitAllocaInst(cast<AllocaInst>(ins));
	} else if (isa<AtomicRMWInst>(ins)) {
		changedType = visitAtomicRMWInst(cast<AtomicRMWInst>(ins));
	} else if (isa<BinaryOperator>(ins)) {
		changedType = visitBinaryOperator(cast<BinaryOperator>(ins));
	} else if (isa<BranchInst>(ins)) {
		changedType = visitBranchInst(cast<BranchInst>(ins));
	} else if (isa<CallInst>(ins)) {
		changedType = visitCallInst(cast<CallInst>(ins));
	} else if (isa<CastInst>(ins)) {
		changedType = visitCastInst(cast<CastInst>(ins));
	} else if (isa<CmpInst>(ins)) {
		changedType = visitCmpInst(cast<CmpInst>(ins));
	} else if (isa<ExtractElementInst>(ins)) {
		changedType = visitExtractElementInst(cast<ExtractElementInst>(ins));
	} else if (isa<ExtractValueInst>(ins)) {
		changedType = visitExtractValueInst(cast<ExtractValueInst>(ins));
	} else if (isa<FenceInst>(ins)) {
		changedType = visitFenceInst(cast<FenceInst>(ins));
	} else if (isa<FreezeInst>(ins)) {
		changedType = visitFreezeInst(cast<FreezeInst>(ins));
	} else if (isa<GetElementPtrInst>(ins)) {
		changedType = visitGetElementPtrInst(cast<GetElementPtrInst>(ins));
	} else if (isa<InsertElementInst>(ins)) {
		changedType = visitInsertElementInst(cast<InsertElementInst>(ins));
	} else if (isa<InsertValueInst>(ins)) {
		changedType = visitInsertValueInst(cast<InsertValueInst>(ins));
	} else if (isa<InvokeInst>(ins)) {
		changedType = visitInvokeInst(cast<InvokeInst>(ins));
	} else if (isa<LandingPadInst>(ins)) {
		changedType = visitLandingPadInst(cast<LandingPadInst>(ins));
	} else if (isa<LoadInst>(ins)) {
		changedType = visitLoadInst(cast<LoadInst>(ins));
	} else if (isa<PHINode>(ins)) {
		changedType = visitPHINode(cast<PHINode>(ins));
	} else if (isa<ReturnInst>(ins)) {
		changedType = visitReturnInst(cast<ReturnInst>(ins));
	} else if (isa<SelectInst>(ins)) {
		changedType = visitSelectInst(cast<SelectInst>(ins));
	} else if (isa<ShuffleVectorInst>(ins)) {
		changedType = visitShuffleVectorInst(cast<ShuffleVectorInst>(ins));
	} else if (isa<StoreInst>(ins)) {
		changedType = visitStoreInst(cast<StoreInst>(ins));
	} else if (isa<SwitchInst>(ins)) {
		changedType = visitSwitchInst(cast<SwitchInst>(ins));
	} else if (isa<UnreachableInst>(ins)) {
		changedType = visitUnreachableInst(cast<UnreachableInst>(ins));
	} else {
		ins->print(errs());
		assert(false && "not valid instruction!");
	}
	return changedType;
}

bool
TypeAnalyzer::visitAllocaInst(AllocaInst* ins) {
	Type* type = ins->getAllocatedType();
	if (!type->isPointerTy()) {
		ValueType* valueType = getValueTypeByType(type);
		if (!type->isArrayTy()) {
			ValueType* pointerType = valueType->toPointer();
			delete valueType;
			valueType = pointerType;
		}
		return assignTypeToValue(ins, valueType);
	}
	return false;
}

bool
TypeAnalyzer::visitAtomicRMWInst(AtomicRMWInst* ins) {
	ValueType* type = getType(ins->getPointerOperand());
	if (type == NULL) {
		return false;
	}
	if (type->isUnion() || type->getType() == PTR_TY) {
		return false;
	}
	ValueType* derefType = type->derefPointer();
	if (derefType->isStruct()) {
		std::vector<Value*> indices;
		indices.push_back(ConstantInt::get(IntegerType::get(ins->getContext(), 32), 0));
		return updateIndicesNoLoop(ins, new ValueType(type->getType(), type, indices));
	} else {
		return assignTypeToValue(ins, derefType);
	}
}

bool
TypeAnalyzer::visitBinaryOperator(BinaryOperator* ins) {
	Value* op1 = ins->getOperand(0);
	return assignTypeToValue(ins, getType(op1));
}

bool
TypeAnalyzer::visitBranchInst(BranchInst* ins) {
	return false;
}

bool
TypeAnalyzer::visitCallBase(CallBase* ins) {
	if (!ins->isIndirectCall() && !ins->isInlineAsm()) {
		Function* callee = ins->getCalledFunction();
		if (callee != NULL && !callee->isIntrinsic() && !callee->isVarArg()) {
			bool changed = false;
			bool argChanged = false;
			// Assign types to args
			for (unsigned i = 0; i < ins->arg_size(); ++i) {
				Value* arg = ins->getArgOperand(i);
				if (assignTypeOnlyToValue(arg, getType(callee->getArg(i)))) {
					changed = true;
				}
				// Assign indices to the function's arguments, if changed, re-analyze the function
				if (updateTypeAndIndices(callee->getArg(i), getType(arg))) {
					argChanged = true;
				}
			}
			// Assign type to return value
			if (assignTypeToValue(ins, getType(callee))) {
				changed = true;
			}
			if (argChanged) {
				// Inter-procedural analysis
				visitedFuncs.insert(callee);
				visitFunction(callee);
			}
			return changed;
		}
	}
	return false;
}

bool
TypeAnalyzer::visitCallInst(CallInst* ins) {
	return visitCallBase(ins);
}

bool
TypeAnalyzer::visitCmpInst(CmpInst* ins) {
	return assignTypeToValue(ins, new ValueType(BOOL_TY));
}

bool
TypeAnalyzer::visitExtractValueInst(ExtractValueInst* ins) {
	Value* aggre = ins->getAggregateOperand();
	Type* type = aggre->getType();
	Type* resultType = ExtractValueInst::getIndexedType(type, ins->getIndices());
	return assignTypeToValue(ins, getValueTypeByType(resultType));
}

bool
TypeAnalyzer::visitFreezeInst(FreezeInst* ins) {
	return assignTypeToValue(ins, getType(ins->getOperand(0)));
}

bool
TypeAnalyzer::visitGetElementPtrInst(GetElementPtrInst* ins) {
	ValueType* sourceTy = getType(ins->getPointerOperand());
	if (sourceTy->getType() == PTR_TY) {
		// ptr type has no use
		return false;
	}
	ValueType* updatedSourceTy = new ValueType(sourceTy->getType(), sourceTy, extractIndices(ins));
	return updateIndicesNoLoop(ins, updatedSourceTy);
}

bool
TypeAnalyzer::visitPHINode(PHINode* ins) {
	bool changed = false;
	for (unsigned i = 0; i < ins->getNumIncomingValues(); ++i) {
		ValueType* type = getType(ins->getIncomingValue(i));
		if (assignTypeToValue(ins, type)) {
			changed = true;
		}
	}
	return changed;
}

bool
TypeAnalyzer::visitReturnInst(ReturnInst* ins) {
	return assignTypeToValue(ins->getReturnValue(), getReturnType(ins->getFunction()));
}

bool
TypeAnalyzer::visitStoreInst(StoreInst* ins) {
	Value* val = ins->getValueOperand();
	if (isa<Function>(val)) {
		// Don't consider function types
		return false;
	}
	ValueType* type = getType(val);
	if (type == NULL || type->getType() == PTR_TY) {
		return false;
	}
	return assignTypeToValue(ins->getPointerOperand(), type->toPointer());
}

bool
TypeAnalyzer::visitLoadInst(LoadInst* ins) {
	ValueType* type = getType(ins->getPointerOperand());
	if (type == NULL) {
		return false;
	}
	if (type->isUnion() || type->getType() == PTR_TY) {
		return false;
	}
	ValueType* derefType = type->derefPointer();
	if (derefType->isStruct()) {
		std::vector<Value*> indices;
		indices.push_back(ConstantInt::get(IntegerType::get(ins->getContext(), 32), 0));
		return updateIndicesNoLoop(ins, new ValueType(type->getType(), type, indices));
	} else {
		return assignTypeToValue(ins, derefType);
	}
}

bool
TypeAnalyzer::visitSwitchInst(SwitchInst* ins) {
	return false;
}

bool
TypeAnalyzer::visitCastInst(CastInst* ins) {
	bool changed = false;
	if (assignTypeToValue(ins->getOperand(0), getValueTypeByType(ins->getSrcTy()))) {
		changed = true;
	}
	if (assignTypeToValue(ins, getValueTypeByType(ins->getDestTy()))) {
		changed = true;
	}
	return changed;
}

bool
TypeAnalyzer::visitUnreachableInst(UnreachableInst* ins) {
	return false;
}

bool
TypeAnalyzer::visitInsertElementInst(InsertElementInst* ins) {
	VectorType* vectorTy = ins->getType();
	return assignTypeToValue(ins, getValueTypeByType(vectorTy));
}

bool
TypeAnalyzer::visitFenceInst(FenceInst* ins) {
	return false;
}

bool
TypeAnalyzer::visitSelectInst(SelectInst* ins) {
	return assignTypeToValue(ins, getType(ins->getTrueValue()));
}

bool
TypeAnalyzer::visitShuffleVectorInst(ShuffleVectorInst* ins) {
	return assignTypeToValue(ins, getValueTypeByType(ins->getType()));
}

bool
TypeAnalyzer::visitExtractElementInst(ExtractElementInst* ins) {
	VectorType* vectorTy = ins->getVectorOperandType();
	return assignTypeToValue(ins, getValueTypeByType(vectorTy->getElementType()));
};

bool
TypeAnalyzer::visitInsertValueInst(InsertValueInst* ins) {
	Value* aggregate = ins->getAggregateOperand();
	return assignTypeToValue(ins, getValueTypeByType(aggregate->getType()));
}

bool
TypeAnalyzer::visitInvokeInst(InvokeInst* ins) {
	return visitCallBase(ins);
}

bool
TypeAnalyzer::visitLandingPadInst(LandingPadInst* ins) {
	return false;
}

ValueType::ValueType(std::string type) {
	this->type = new UnifiedType(type);
}

ValueType::ValueType(std::string type, ValueType* parent, std::vector<Value*> indices) {
	if (parent->getType() == PTR_TY) {
		errs() << "ptr source\n";
		assert(false);
	}
	this->type = new UnifiedType(type);
	addNestedIndices(parent, indices);
}

bool
ValueType::equal(const ValueType* valueType) const {
	if (this->type->compare(valueType->type) == 0) {
		if (this->nested_indices.size() == 0 && valueType->nested_indices.size() == 0) {
			return true;
		}
		if (this->nested_indices.size() != valueType->nested_indices.size()) {
			return false;
		}
		for (const auto& i : this->nested_indices) {
			if (valueType->nested_indices.count(i.first) > 0) {
				const std::vector<std::vector<Value*>>& indices_b = valueType->nested_indices.at(i.first);
				const std::vector<std::vector<Value*>>& indices_a = i.second;
				for (const std::vector<Value*>& indices_b1 : indices_b) {
					if (!indicesExist(indices_a, indices_b1)) {
						return false;
					}
				}
			} else {
				return false;
			}
		}
		return true;
	}
	return false;
}

bool
ValueType::possibleAlias(const ValueType* valueType) const {
	if (this->nested_indices.size() == 0 && valueType->nested_indices.size() == 0) {
		return this->type->compare(valueType->type) == 0;
	}
	for (const auto& i : this->nested_indices) {
		if (valueType->nested_indices.count(i.first) > 0) {
			const std::vector<std::vector<Value*>>& indices_b = valueType->nested_indices.at(i.first);
			const std::vector<std::vector<Value*>>& indices_a = i.second;
			for (const std::vector<Value*>& indices_b1 : indices_b) {
				for (const std::vector<Value*>& indices_a1 : indices_a) {
					if (indices_a1.size() == indices_b1.size()) {
						if (indicesAlias(indices_b1, indices_a1)) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool
ValueType::mergeIndices(ValueType* type) {
	bool typeChanged = false;
	for (auto& i : type->nested_indices) {
		if (this->nested_indices.count(i.first) > 0) {
			std::vector<std::vector<Value*>>& indices1 = this->nested_indices[i.first];
			std::vector<std::vector<Value*>>& indices2 = i.second;
			for (unsigned j = 0; j < indices2.size(); ++j) {
				if (!indicesExist(indices1, indices2[j])) {
					indices1.push_back(indices2[j]);
					typeChanged = true;
				}
			}
		} else {
			this->nested_indices[i.first] = i.second;
			typeChanged = true;
		}
	}
	return typeChanged;
}

bool
ValueType::mergeIndicesNoLoop(ValueType* type) {
	bool typeChanged = false;
	for (auto& i : type->nested_indices) {
		if (this->nested_indices.count(i.first) > 0) {
			std::vector<std::vector<Value*>>& indices1 = this->nested_indices[i.first];
			std::vector<std::vector<Value*>>& indices2 = i.second;
			bool hasLoop = false;
			for (unsigned j = 0; j < indices2.size(); ++j) {
				if (indicesExist(indices1, indices2[j])) {
					hasLoop = true;
					break;
				}
			}
			if (!hasLoop) {
				for (unsigned j = 0; j < indices2.size(); ++j) {
					indices1.push_back(indices2[j]);
					typeChanged = true;
				}
			}
		} else {
			this->nested_indices[i.first] = i.second;
			typeChanged = true;
		}
	}
	return typeChanged;
}

bool
ValueType::mergeValueType(ValueType* type) {
	// Deal with conflicts
	bool typeChanged = false;
	if (this->mergeValueTypeOnly(type)) {
		typeChanged = true;
	}
	if (this->mergeIndices(type)) {
		typeChanged = true;
	}
	return typeChanged;
}

bool
ValueType::mergeValueTypeOnly(ValueType* type) {
	// Deal with conflicts
	if (this->type->compare(type->type) < 0) {
		this->type = type->type;
		return true;
	}
	return false;
}

bool
ValueType::indicesOverlapped(ValueType* type) {
	std::unordered_map<std::string, std::vector<std::vector<Value*>>>& otherIndices = type->nested_indices;
	for (auto& itr : this->nested_indices) {
		if (otherIndices.count(itr.first) > 0) {
			std::vector<std::vector<Value*>>& indicesList = itr.second;
			std::vector<std::vector<Value*>>& otherIndicesList = otherIndices[itr.first];
			for (std::vector<Value*>& indices : indicesList) {
				if (indicesExist(otherIndicesList, indices)) {
					return true;
				}
			}
		}
	}
	return false;
}

void
ValueType::addNestedIndices(ValueType* parent, std::vector<Value*> indices) {
	if (this->nested_indices.count(parent->getType()) == 0) {
		this->nested_indices[parent->getType()].push_back(indices);
	} else {
		std::vector<std::vector<Value*>>& existed = this->nested_indices[parent->getType()];
		if (!indicesExist(existed, indices)) {
			existed.push_back(indices);
		}
	}
	// Add aliases
	for (auto& i : parent->nested_indices) {
		std::vector<std::vector<Value*>> previousIndices = i.second;
		if (this->nested_indices.count(i.first) > 0) {
			std::vector<std::vector<Value*>>& existed = this->nested_indices[i.first];
			for (std::vector<Value*>& previous : previousIndices) {
				previous.insert(previous.end(), indices.begin(), indices.end());
				if (!indicesExist(existed, previous)) {
					existed.push_back(previous);
				}
			}
		} else {
			for (std::vector<Value*>& previous : previousIndices) {
				previous.insert(previous.end(), indices.begin(), indices.end());
			}
			this->nested_indices[i.first] = previousIndices;
		}
	}
}

ValueType*
ValueType::getValueTypePlusIndices(std::vector<Value*> indices) {
	ValueType* newType = new ValueType(this->type->getName());
	newType->nested_indices = this->nested_indices;
	for (auto& i : newType->nested_indices) {
		std::vector<std::vector<Value*>>& existed = i.second;
		for (std::vector<Value*>& existedInd : existed) {
			existedInd.insert(existedInd.end(), indices.begin(), indices.end());
		}
	}
	return newType;
}

ValueType*
ValueType::getValueTypePlusIndices(std::vector<Value*> indices, ValueType* excluded) {
	if (excluded == NULL) {
		return getValueTypePlusIndices(indices);
	}
	ValueType* newType = new ValueType(this->type->getName());
	std::unordered_map<std::string, std::vector<std::vector<Value*>>>& excludedIndices = excluded->nested_indices;
	for (auto i : this->nested_indices) {
		std::vector<std::vector<Value*>> existedList = i.second;
		if (excludedIndices.count(i.first) > 0) {
			std::vector<std::vector<Value*>>& excludedList = excludedIndices[i.first];
			// Only add indices not excluded
			for (std::vector<Value*> existedInd : existedList) {
				if (!indicesExist(excludedList, existedInd)) {
					existedInd.insert(existedInd.end(), indices.begin(), indices.end());
					newType->nested_indices[i.first].push_back(existedInd);
				}
			}
		} else {
			for (std::vector<Value*>& existedInd : existedList) {
				existedInd.insert(existedInd.end(), indices.begin(), indices.end());
			}
			newType->nested_indices[i.first] = existedList;
		}
	}
	return newType;
}

ValueType*
ValueType::toPointer() {
	ValueType* newType = new ValueType(this->type->getName());
	newType->nested_indices = this->nested_indices;
	delete newType->type;
	newType->type = this->type->getReference();
	return newType;
}

ValueType*
ValueType::derefPointer() {
	ValueType* newType = new ValueType(this->type->getName());
	newType->nested_indices = this->nested_indices;
	delete newType->type;
	if (this->type->isPointer()) {
		newType->type = this->type->dereference();
	} else if (this->type->isArray()) {
		newType->type = this->type->getArrayElement();
	} else {
		newType->type = this->type;
	}
	return newType;
}

bool
ValueType::fieldOfType(std::string type) {
	if (this->type->getName() == type || this->type->getName() == type + "*") {
		return true;
	}
	for (auto& it : this->nested_indices) {
		if (it.first == type || it.first == type + "*") {
			return true;
		}
	}
	return false;
}

ValueType::UnifiedType::UnifiedType(std::string name) {
	this->name = name;
	if (name == "unsigned long long") {
		name = UNSIGNED_LONG_TY;
	}
	if (name.back() == '*') {
		if (name == "void*" || name == "unsigned char*") {
			this->category = VOID_POINTER;
		} else {
			this->category = POINTER;
		}
		this->len = POINTER_LEN;
	} else if (name == PTR_TY) {
		this->category = POINTER;
		this->len = POINTER_LEN;
	} else if (name.back() == ']') {
		this->category = ARRAY;
		this->len = POINTER_LEN;
	} else if (name.back() == '>') {
		this->category = VECTOR;
		this->len = POINTER_LEN;
	} else if (name.back() == '}') {
		this->category = UNION;
		std::regex re("\\{(\\d+)\\}");
		std::smatch match;
		if (std::regex_search(name, match, re) && match.size() > 1) {
			std::string len = match.str(1);
			this->len = std::stoi(len);
		} else {
			this->len = POINTER_LEN;
		}
	} else {
		if (name == VOID_TY) {
			this->category = VOID;
			this->len = 0;
		} else if (name == BOOL_TY) {
			this->category = UNSIGNED_INT;
			this->len = 1;
		} else if (name == CHAR_TY) {
			this->category = SIGNED_INT;
			this->len = 8;
		} else if (name == UNSIGNED_CHAR_TY) {
			this->category = UNSIGNED_INT;
			this->len = 8;
		} else if (name == SHORT_TY) {
			this->category = SIGNED_INT;
			this->len = 16;
		} else if (name == UNSIGNED_SHORT_TY) {
			this->category = UNSIGNED_INT;
			this->len = 16;
		} else if (name == INTEGER_TY) {
			this->category = SIGNED_INT;
			this->len = 32;
		} else if (name == UNSIGNED_INTEGER_TY) {
			this->category = UNSIGNED_INT;
			this->len = 32;
		} else if (name == LONG_TY) {
			this->category = SIGNED_INT;
			this->len = 64;
		} else if (name == UNSIGNED_LONG_TY) {
			this->category = UNSIGNED_INT;
			this->len = 64;
		} else if (name == FLOAT_TY) {
			this->category = SIGNED_FLO;
			this->len = 32;
		} else if (name == DOUBLE_TY) {
			this->category = SIGNED_FLO;
			this->len = 64;
		} else {
			this->category = STRUCT;
			this->len = -1;
		}
	}
}

int
ValueType::UnifiedType::compare(const UnifiedType* ty) const {
	if (this->name == ty->name || (this->category == VOID_POINTER && ty->category == VOID_POINTER)) {
		return 0;
	}
	if (this->category == UNION || ty->category == UNION) {
		return this->len == ty->len;
	}
	if (this->category == VOID_POINTER && ty->category != VOID_POINTER) {
		return -1;
	}
	if (this->category != VOID_POINTER && ty->category == VOID_POINTER) {
		return 1;
	}
	if (this->category == POINTER && this->name == PTR_TY) {
		return -1;
	}
	if (ty->category == POINTER && ty->name == PTR_TY) {
		return 1;
	}
	if (this->category == STRUCT && this->name == OBJECT_TY && ty->category == STRUCT) {
		return -1;
	}
	if (ty->category == STRUCT && ty->name == OBJECT_TY && this->category == STRUCT) {
		return 1;
	}
	if (this->category == VECTOR) {
		return this->getVectorElement()->compare(ty);
	}
	if (ty->category == VECTOR) {
		return this->compare(ty->getVectorElement());
	}
	if (this->category == POINTER && ty->category == POINTER) {
		return this->dereference()->compare(ty->dereference());
	}
	if (this->category == ARRAY && ty->category == ARRAY) {
		return this->getArrayElement()->compare(ty->getArrayElement());
	}
	if (this->category == POINTER && ty->category == ARRAY) {
		return this->dereference()->compare(ty->getArrayElement());
	}
	if (this->category == ARRAY && ty->category == POINTER) {
		return this->getArrayElement()->compare(ty->dereference());
	}
	if (this->category == VOID) {
		return -1;
	}
	if (ty->category == VOID) {
		return 1;
	}
	if (this->category == SIGNED_INT && ty->category == UNSIGNED_INT) {
		return 1;
	}
	if (this->category == UNSIGNED_INT && ty->category == SIGNED_INT) {
		return -1;
	}
	if (this->category == SIGNED_FLO && ty->category == UNSIGNED_FLO) {
		return 1;
	}
	if (this->category == UNSIGNED_FLO && ty->category == SIGNED_FLO) {
		return -1;
	}
	if (this->category == ty->category) {
		if (this->len < ty->len) {
			return 1;
		} else {
			return -1;
		}
	}
	if (this->category == STRUCT && ty->category == UNSIGNED_INT && ty->len == 8) {
		return 1;
	}
	if (ty->category == STRUCT && this->category == UNSIGNED_INT && this->len == 8) {
		return -1;
	}
	if (this->category == STRUCT || ty->category == STRUCT) {
		return 0;
	}
	if (this->category == POINTER) {
		if (ty->category == UNSIGNED_INT && ty->len == POINTER_LEN) {
			return 0;
		} else {
			return 1;
		}
	}
	if (ty->category == POINTER) {
		if (this->category == UNSIGNED_INT && this->len == POINTER_LEN) {
			return 0;
		} else {
			return -1;
		}
	}
	if (this->category == ARRAY) {
		if (ty->category == UNSIGNED_INT && ty->len == POINTER_LEN) {
			return 0;
		}
		if (ty->category == UNSIGNED_INT || ty->category == SIGNED_INT) {
			return 1;
		}
	}
	if (ty->category == ARRAY) {
		if (this->category == UNSIGNED_INT && this->len == POINTER_LEN) {
			return 0;
		}
		if (this->category == UNSIGNED_INT || this->category == SIGNED_INT) {
			return -1;
		}
	}
	errs() << "failed to compare " << this->name << " " << this->category << " " << ty->name << " " << ty->category << "\n";
	assert(false && "Unhandled type comparison\n");
	return 0;
}

bool
ValueType::indicesAlias(const std::vector<Value*>& indices1, const std::vector<Value*>& indices2) const {
	if (indices1.size() != indices2.size()) {
		return false;
	}
	for (unsigned i = 0; i < indices1.size(); ++i) {
		if (isa<ConstantInt>(indices1[i]) && isa<ConstantInt>(indices2[i])) {
			ConstantInt* consInt1 = cast<ConstantInt>(indices1[i]);
			ConstantInt* consInt2 = cast<ConstantInt>(indices2[i]);
			if (consInt1->getZExtValue() != consInt2->getZExtValue()) {
				return false;
			}
		}
	}
	return true;
}

bool
ValueType::indicesEqual(const std::vector<Value*>& indices1, const std::vector<Value*>& indices2) const {
	if (indices1.size() != indices2.size()) {
		return false;
	}
	for (unsigned i = 0; i < indices1.size(); ++i) {
		if (isa<ConstantInt>(indices1[i]) && isa<ConstantInt>(indices2[i])) {
			ConstantInt* consInt1 = cast<ConstantInt>(indices1[i]);
			ConstantInt* consInt2 = cast<ConstantInt>(indices2[i]);
			if (consInt1->getZExtValue() != consInt2->getZExtValue()) {
				return false;
			}
		} else {
			if (indices1[i] != indices2[i]) {
				return false;
			}
		}
	}
	return true;
}

// Whether a vector of indices exists in a vector of vectors of indices.
bool
ValueType::indicesExist(const std::vector<std::vector<Value*>>& indicesVec, const std::vector<Value*>& indices) const {
	for (const std::vector<Value*>& indices1 : indicesVec) {
		if (indicesEqual(indices1, indices)) {
			return true;
		}
	}
	return false;
}
