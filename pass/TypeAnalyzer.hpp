#pragma once

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace llvm;

#define VOID_TY "void"
#define BOOL_TY "bool"
#define CHAR_TY "char"
#define UNSIGNED_CHAR_TY "unsigned char"
#define SHORT_TY "short"
#define UNSIGNED_SHORT_TY "unsigned short"
#define UNSIGNED_INT24_TY "unsigned int24"
#define INTEGER_TY "int"
#define UNSIGNED_INTEGER_TY "unsigned int"
#define LONG_TY "long"
#define UNSIGNED_LONG_TY "unsigned long"
#define UNSIGNED_INT56_TY "unsigned int56"
#define UNSIGNED_INT96_TY "unsigned int96"
#define UNSIGNED_INT128_TY "unsigned int128"
#define FLOAT_TY "float"
#define DOUBLE_TY "double"
#define PTR_TY "ptr"
#define OBJECT_TY "Object"
#define EMPTY_TY ""

class ValueType {
public:
	ValueType(std::string type);
	ValueType(std::string type, ValueType* parent, std::vector<Value*> indices);
	bool equal(const ValueType* valueType) const;
	bool possibleAlias(const ValueType* valueType) const;
	std::string getType() {
		return this->type->getName();
	}
	void setType(std::string type) {
		delete this->type;
		this->type = new UnifiedType(type);
	}
	bool mergeIndices(ValueType* type);
	bool mergeIndicesNoLoop(ValueType* type);
	bool mergeValueType(ValueType* type);
	bool mergeValueTypeOnly(ValueType* type);
	bool indicesOverlapped(ValueType* type);
	void addNestedIndices(ValueType* parent, std::vector<Value*> indices);
	ValueType* getValueTypePlusIndices(std::vector<Value*> indices);
	ValueType* getValueTypePlusIndices(std::vector<Value*> indices, ValueType* excluded);
	ValueType* toPointer();
	ValueType* derefPointer();
	bool fieldOfType(std::string type);
	bool isArray() {
		return this->type->isArray();
	}
	bool isStruct() {
		return this->type->isStruct();
	}
	bool isUnion() {
		return this->type->isUnion();
	}
	void print() {
		errs() << type->getName() << "\n";
		for (auto itr : nested_indices) {
			errs() << itr.first << "\n";
			for (std::vector<Value*>& indices : itr.second) {
				errs() << "indices\n";
				for (Value* v : indices) {
					v->print(errs());
					errs() << "\n";
				}
			}
		}
	}
private:
	class UnifiedType {
	public:
		UnifiedType(std::string name);
		int compare(const UnifiedType* ty) const;
		UnifiedType* getReference() {
			return new UnifiedType(this->name + "*");
		}
		UnifiedType* dereference() const {
			assert(this->category == POINTER || this->category == VOID_POINTER);
			return new UnifiedType(this->name.substr(0, this->name.size() - 1));
		}
		UnifiedType* getArrayElement() const {
			assert(this->category == ARRAY);
			std::size_t found = this->name.find_last_of('[');
			return new UnifiedType(this->name.substr(0, found));
		}
		UnifiedType* getVectorElement() const {
			assert(this->category == VECTOR);
			std::size_t found = this->name.find_last_of('<');
			return new UnifiedType(this->name.substr(0, found));
		}
		std::string getName() const {
			return name;
		}
		bool isPointer() {
			return this->category == POINTER || this->category == VOID_POINTER;
		}
		bool isArray() {
			return this->category == ARRAY;
		}
		bool isStruct() {
			return this->category == STRUCT;
		}
		bool isUnion() {
			return this->category == UNION;
		}
	private:
		enum Category {
			VOID, SIGNED_INT, SIGNED_FLO, UNSIGNED_INT, UNSIGNED_FLO, POINTER, VOID_POINTER, ARRAY, STRUCT, VECTOR, UNION
		};
		static const int POINTER_LEN = 64;
		std::string name;
		Category category;
		int len;
	};
	UnifiedType* type;
	std::unordered_map<std::string, std::vector<std::vector<Value*>>> nested_indices;
	bool indicesAlias(const std::vector<Value*>& indices1, const std::vector<Value*>& indices2) const;
	bool indicesEqual(const std::vector<Value*>& indices1, const std::vector<Value*>& indices2) const;
	bool indicesExist(const std::vector<std::vector<Value*>>& indicesVec, const std::vector<Value*>& indices) const;
};
class TypeAnalyzer {
public:
	TypeAnalyzer(Module* M);
	void analyze();
	std::string getValueType(Value* value);
	ValueType* getValueVType(Value* value);
	std::unordered_set<Value*> getTypeIntersection(std::unordered_set<Value*> set1, std::unordered_set<Value*> set2);
	bool fieldOfType(Value* v, std::string type);
	bool fieldOfTypes(Value* v, std::unordered_set<std::string> types);
private:
	Module* M;
	std::unordered_map<Value*, ValueType*> types;
	std::unordered_map<Function*, ValueType*> returnTypes;
	std::unordered_set<Function*> visitedFuncs;
	// Phases
	void collectTypeInfo();
	void propagateTypeInfo();

	// Subprocedures
	void setFuncSignatureTypes(Function* func);
	void visitFunction(Function* func);

	// Utilities
	bool updateIndices(Value* val, ValueType* type);
	bool updateIndicesNoLoop(Value* val, ValueType* type);
	bool updateTypeAndIndices(Value* val, ValueType* type);
	bool assignTypeToValue(Value* val, ValueType* type);
	bool assignTypeOnlyToValue(Value* val, ValueType* type);
	bool assignReturnTypeToFunc(Function* func, ValueType* type);
	std::string typeToString(Type* type);
	ValueType* getValueTypeByType(Type* type);
	ValueType* getValueTypeByType(Type* type, ValueType* parent, std::vector<Value*> indices);
	std::string DITypeToString(DIType* type);
	std::vector<Value*> extractIndices(GetElementPtrInst* gep);
	inline ValueType* getType(Value* val);
	inline ValueType* getReturnType(Function* func);

	// Visits
	bool visitBB(BasicBlock* BB);
	bool visitInstruction(Instruction* ins);
	bool visitAllocaInst(AllocaInst* ins);
	bool visitAtomicRMWInst(AtomicRMWInst* ins);
	bool visitBinaryOperator(BinaryOperator* ins);
	bool visitBranchInst(BranchInst* ins);
	bool visitCallBase(CallBase* ins);
	bool visitCallInst(CallInst* ins);
	bool visitCmpInst(CmpInst* ins);
	bool visitExtractValueInst(ExtractValueInst* ins);
	bool visitFreezeInst(FreezeInst* ins);
	bool visitGetElementPtrInst(GetElementPtrInst* ins);
	bool visitPHINode(PHINode* ins);
	bool visitReturnInst(ReturnInst* ins);
	bool visitStoreInst(StoreInst* ins);
	bool visitLoadInst(LoadInst* ins);
	bool visitSwitchInst(SwitchInst* ins);
	bool visitCastInst(CastInst* ins);
	bool visitUnreachableInst(UnreachableInst* ins);
	bool visitInsertElementInst(InsertElementInst* ins);
	bool visitFenceInst(FenceInst* ins);
	bool visitSelectInst(SelectInst* ins);
	bool visitShuffleVectorInst(ShuffleVectorInst* ins);
	bool visitExtractElementInst(ExtractElementInst* ins);
	bool visitInsertValueInst(InsertValueInst* ins);
	bool visitInvokeInst(InvokeInst* ins);
	bool visitLandingPadInst(LandingPadInst* ins);
};
