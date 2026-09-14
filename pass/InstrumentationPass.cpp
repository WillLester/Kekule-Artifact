#include "DeviceBasicAnalyzer.hpp"
#include "QemuBasicAnalyzer.hpp"
#include "VBoxBasicAnalyzer.hpp"
#include "InstrumentationPass.hpp"
#include "TaintAnalyzer.hpp"
#include "AliasTaintAnalyzer.hpp"
#include "TypeAnalyzer.hpp"
#include "RegBehaviorAnalyzer.hpp"
#include "DependencyAnalyzer.hpp"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include <stack>
#include <vector>

#define QEMU_SIM_ADDR_IDX 1
#define VBOX_SIM_ADDR_IDX 2

using namespace llvm;

static cl::opt<std::string> platform("platform",
	cl::init(""),
	cl::desc("The hypervisor you are testing."),
	cl::value_desc("The hypervisor you are testing."));

PreservedAnalyses
InstrumentationPass::run(Module& M, ModuleAnalysisManager& AM) {
	DeviceBasicAnalyzer* basicAnalyzer;
	if (platform == "QEMU") {
		basicAnalyzer = new QemuBasicAnalyzer();
	} else if (platform == "VBox") {
		basicAnalyzer = new VBoxBasicAnalyzer();
	} else {
		errs() << "Not supported hypervisor\n";
		return PreservedAnalyses::all();
	}
	stateTypeNames = basicAnalyzer->getDevStructTypeNames(M);
	assert(!stateTypeNames.empty());
	writeSims = basicAnalyzer->findWriteSimulations(M, stateTypeNames);
	if (writeSims.empty()) {
		errs() << "No write simulations found\n";
		assert(false);
	}
	std::vector<FuncArg*> simFuncs;
	for (Function* writeSim : writeSims) {
		FuncArg* funcArg;
		if (platform == "QEMU") {
			funcArg = new FuncArg(writeSim, QEMU_SIM_ADDR_IDX);
		} else if (platform == "VBox") {
			funcArg = new FuncArg(writeSim, VBOX_SIM_ADDR_IDX);
		}
		simFuncs.push_back(funcArg);
	}
	TypeAnalyzer* typeAnalyzer = new TypeAnalyzer(&M);
	typeAnalyzer->analyze();
	regAnalyzer = new RegBehaviorAnalyzer(simFuncs, stateTypeNames, typeAnalyzer, platform);
	std::vector<RegBehavior*> behaviors = regAnalyzer->analyze();
	DependencyAnalyzer* dependAnalyzer = new DependencyAnalyzer(behaviors);
	dependAnalyzer->analyze();
	// Create the instrumentation function
	createFeedbackFunc(M);
	std::unordered_map<BasicBlock*, std::vector<Var*>> accesses;
	for (RegBehavior* behavior : behaviors) {
	  errs() << behavior->getId() << ":\n";
	  for (auto& itr : behavior->getAccessedVars()) {
	  	BasicBlock* BB = itr.first;
	  	std::vector<Var*> vars = itr.second;
	  	if (!vars.empty()) {
	  		BB->print(errs());
	  		regAnalyzer->printDebugLineAndCol(BB->getTerminator());
	  		errs() << "\n";
	  		for (Var* var : vars) {
	  			var->print();
				regAnalyzer->printDebugLineAndCol(var->getIns());
	  		}
	  	}
	  }
	}
	for (RegBehavior* behavior : behaviors) {
		std::unordered_map<BasicBlock*, std::vector<Var*>> accessedVars = behavior->getAccessedVars();
		for (auto& itr : accessedVars) {
			std::vector<Var*>& blkAccesses = accesses[itr.first];
			blkAccesses.insert(blkAccesses.end(), itr.second.begin(), itr.second.end());
		}
	}
	std::unordered_map<BasicBlock*, std::vector<Var*>> points = findInstrumentationPoints(accesses);
	instrument(behaviors, points);
	cleanup(M);
	bool brokenDebug = false;
	if (verifyModule(M, &errs(), &brokenDebug)) {
		errs() << "Failed\n";
		if (brokenDebug) {
			errs() << "Debug broken\n";
		}
	}
	errs() << "All done\n";
	return PreservedAnalyses::all();
}

void
InstrumentationPass::createFeedbackFunc(Module& M) {
	LLVMContext& context = M.getContext();
	Type* returnType = Type::getVoidTy(context);
	std::vector<Type*> argTypes({Type::getInt16Ty(context), Type::getInt8Ty(context), Type::getInt1Ty(context), Type::getInt1Ty(context)});
	FunctionType* funcType = FunctionType::get(returnType, argTypes, false);
	feedback = Function::Create(funcType, Function::ExternalLinkage, "__sanitizer_reg_trace_push", M);
}

bool
InstrumentationPass::isMarked(LoadInst* load) {
	AliasTaintAnalyzer analyzer(load->getFunction(), load);
	analyzer.analyze();
	std::unordered_set<Value*> tainted = analyzer.getTaintedValues();
	for (Value* v : tainted) {
		for (Value::user_iterator i = v->user_begin(); i != v->user_end(); ++i) {
			if (isa<Instruction>(*i)) {
				Instruction* ins = cast<Instruction>(*i);
				if (isa<BinaryOperator>(ins)) {
					BinaryOperator* biOp = cast<BinaryOperator>(ins);
					BinaryOperator::BinaryOps op = biOp->getOpcode();
					// Divider
					if (op == Instruction::SDiv || op == Instruction::UDiv || op == Instruction::FDiv) {
						if (v == biOp->getOperand(1)) {
							return true;
						}
					}
				} else if (isa<GetElementPtrInst>(ins)) {
					// Index
					GetElementPtrInst* gep = cast<GetElementPtrInst>(ins);
					for (GetElementPtrInst::op_iterator itr = gep->idx_begin(); itr != gep->idx_end(); ++itr) {
						if (v == *itr) {
							return true;
						}
					}
				} else if (isa<CallInst>(ins)) {
					// Read len
				} else if (isa<IntToPtrInst>(ins)) {
					// Deref
					for (Value::user_iterator j = ins->user_begin(); j != ins->user_end(); ++j) {
						if (isa<LoadInst>(*j)) {
							// Deref from an integer
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

void
InstrumentationPass::markVars(std::unordered_map<BasicBlock*, std::vector<Var*>>& accesses) {
	for (auto& itr : accesses) {
		std::vector<Var*>& vars = itr.second;
		for (Var* var : vars) {
			Instruction* ins = var->getIns();
			if (isa<LoadInst>(ins)) {
				LoadInst* load = cast<LoadInst>(ins);
				if (isMarked(load)) {
					errs() << "Mark an instruction\n";
					load->print(errs());
					var->setVarMarked(true);
				}
			}
		}
	}
}

std::unordered_map<BasicBlock*, std::vector<Var*>>
InstrumentationPass::findInstrumentationPoints(std::unordered_map<BasicBlock*, std::vector<Var*>> accesses) {
	for (auto& itr1 : accesses) {
		BasicBlock* BB1 = itr1.first;
		PostDominatorTree* domTree = regAnalyzer->getPostDominatorTree(BB1->getParent());
		assert(domTree != NULL && "no post dom tree");
		DomTreeNodeBase<BasicBlock>* node1 = domTree->getNode(BB1);
		std::vector<Var*>& vars1 = itr1.second;
		for (auto& itr2 : accesses) {
			BasicBlock* BB2 = itr2.first;
			if (BB1 == BB2 || BB2->getParent() != BB1->getParent()) {
				continue;
			}
			DomTreeNodeBase<BasicBlock>* node2 = domTree->getNode(BB2);
			std::vector<Var*>& vars2 = itr2.second;
			if (domTree->dominates(node1, node2)) {
				// BB1 post-dominates BB2, we can remove the duplicated instrumentation points
				std::vector<bool> deleted(vars2.size(), false);
				for (unsigned i = 0; i < vars1.size(); ++i) {
					Var* var1 = vars1[i];
					for (unsigned j = 0; j < vars2.size(); ++j) {
						Var* var2 = vars2[j];
						if (var1->equal(var2)) {
							deleted[j] = true;
						}
					}
				}
				for (int i = vars2.size() - 1; i >= 0; --i) {
					if (deleted[i]) {
						vars2.erase(vars2.begin() + i);
					}
				}
			}
		}
	}
	// Need to remove duplicates
	for (auto& itr : accesses) {
		std::vector<Var*>& vars = itr.second;
		std::vector<int> deleted;
		std::unordered_set<Var*, Var::VarHash, Var::VarEqual> existed;
		for (unsigned i = 0; i < vars.size(); ++i) {
			Var* var = vars[i];
			if (existed.count(var) > 0) {
				deleted.push_back(i);
			} else {
				existed.insert(var);
			}
		}
		for (int i = deleted.size() - 1; i >= 0; --i) {
			int idx = deleted[i];
			vars.erase(vars.begin() + idx);
		}
	}
	return accesses;
}

void
InstrumentationPass::instrument(std::vector<RegBehavior*> behaviors, std::unordered_map<BasicBlock*, std::vector<Var*>> points) {
	for (RegBehavior* behavior : behaviors) {
		std::vector<Instruction*> begin = behavior->getBegin();
		for (Instruction* ins : begin) {
			BasicBlock* BB = ins->getParent();
			IRBuilder<> builder(BB->getContext());
			Instruction* first = BB->getFirstNonPHI();
			assert(first != NULL && "BB has a null first ins\n");
			builder.SetInsertPoint(first);
			Value* arg1 = builder.getInt16(behavior->getId());
			Value* arg2 = builder.getInt8(1);
			Value* arg3 = builder.getInt1(true);
			Value* arg4 = builder.getInt1(false);
			builder.CreateCall(feedback, {arg1, arg2, arg3, arg4});
		}
		std::unordered_set<Instruction*> end = behavior->getEnd();
		for (Instruction* ins : end) {
			BasicBlock* BB = ins->getParent();
			IRBuilder<> builder(BB->getContext());
			Instruction* term = BB->getTerminator();
			assert(term != NULL && "BB has a null term ins\n");
			builder.SetInsertPoint(term);
			Value* arg1 = builder.getInt16(behavior->getId());
			Value* arg2 = builder.getInt8(1);
			Value* arg3 = builder.getInt1(false);
			Value* arg4 = builder.getInt1(false);
			builder.CreateCall(feedback, {arg1, arg2, arg3, arg4});
		}
	}
	for (auto& itr : points) {
		BasicBlock* BB = itr.first;
		IRBuilder<> builder(BB->getContext());
		Instruction* term = BB->getTerminator();
		if (term) {
			builder.SetInsertPoint(term);
		} else {
			builder.SetInsertPoint(BB);
		}
		for (Var* var : itr.second) {
			Value* arg1 = builder.getInt16(var->getId());
			Value* arg2 = builder.getInt8(2);
			Value* arg3 = builder.getInt1(var->isVarRead());
			Value* arg4 = builder.getInt1(var->isVarMarked());
			builder.CreateCall(feedback, {arg1, arg2, arg3, arg4});
		}
	}
}

/**
  * An add-hoc function to remove redundant symbols.
  **/
void
InstrumentationPass::cleanup(Module& M) {
	std::vector<Function*> removed;
	for (Function& F : M) {
		if (F.hasName() && F.getName().startswith("sancov.module_ctor_8bit_counters") && !F.getName().equals("sancov.module_ctor_8bit_counters")) {
			removed.push_back(&F);
		}
	}
	GlobalVariable *llvmUsed = M.getGlobalVariable("llvm.used", true);
	if (llvmUsed && !llvmUsed->isDeclaration()) {
		ConstantArray *CA = dyn_cast<ConstantArray>(llvmUsed->getInitializer());
		if (CA) {
			std::vector<Constant*> NewValues;
			for (unsigned i = 0; i < CA->getNumOperands(); ++i) {
				Constant* V = CA->getAggregateElement(i);
				if (Function* F = dyn_cast<Function>(V)) {
					if (std::find(removed.begin(), removed.end(), F) == removed.end()) {
						NewValues.push_back(V); // Keep this element
					} else {
					}
				} else {
					NewValues.push_back(V); // Not a function, keep the Global
				}
			}
			if (NewValues.size() != CA->getNumOperands()) { // Only update if there are changes
				ArrayType *NewArrayType = ArrayType::get(CA->getType()->getElementType(), NewValues.size());
				llvmUsed->eraseFromParent(); // Remove the old llvm.used

				GlobalVariable *newLLVMUsed =
					new GlobalVariable(M, NewArrayType, false, GlobalValue::AppendingLinkage,
							   ConstantArray::get(NewArrayType, NewValues), "llvm.used");
				newLLVMUsed->setSection("llvm.metadata");
			}
		}
	}

    // Assuming llvmUsed is a i8* array
	GlobalVariable *llvmCtors = M.getGlobalVariable("llvm.global_ctors", true);
	if (llvmCtors && !llvmCtors->isDeclaration()) {
		ConstantArray* CA = dyn_cast<ConstantArray>(llvmCtors->getInitializer());
		if (CA) {
			std::vector<Constant*> NewValues;
			for (unsigned i = 0; i < CA->getNumOperands(); ++i) {
				Constant* V = CA->getAggregateElement(i);
				if (ConstantStruct* SV = dyn_cast<ConstantStruct>(V)) {
					if (Function* F = dyn_cast<Function>(SV->getOperand(1))) {
						if (std::find(removed.begin(), removed.end(), F) == removed.end()) {
							NewValues.push_back(V); // Keep this element
						}
					} else {
						NewValues.push_back(V); // Not a function, keep the Global
					}
				}
			}
			if (NewValues.size() != CA->getNumOperands()) { // Only update if there are changes
				ArrayType *NewArrayType = ArrayType::get(CA->getType()->getElementType(), NewValues.size());
				llvmCtors->eraseFromParent(); // Remove the old llvm.used

				new GlobalVariable(M, NewArrayType, false, GlobalValue::AppendingLinkage,
						   ConstantArray::get(NewArrayType, NewValues), "llvm.global_ctors");
			}
		}
	}

	for (Function* F : removed) {
		F->eraseFromParent();
	}
}
