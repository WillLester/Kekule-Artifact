#include "VBoxIndirectCallAnalyzer.hpp"
#include "VBoxBasicAnalyzer.hpp"
#include "llvm/IR/Constants.h"

#define WRITE_IDX 2

std::unordered_set<Function*>
VBoxIndirectCallAnalyzer::getTarget(CallBase* ins) {
    std::unordered_set<Function*> res;
    if (callMap.count(ins) == 0) {
        return res;
    }
    std::unordered_set<GlobalVariable*> GVs = callMap[ins];
    for (GlobalVariable* GV : GVs) {
        Constant* init = GV->getInitializer();
        if (!isa<ConstantArray>(init)) {
            init = cast<Constant>(init->getOperand(0));
        }
        assert(isa<ConstantArray>(init) && "registers are not an array");
        for (unsigned i = 0; i < init->getNumOperands(); ++i) {
            Value* operand = init->getOperand(i);
            assert(isa<ConstantStruct>(operand) && "a register is not a struct");
            ConstantStruct* opStruct = cast<ConstantStruct>(operand);
            Value* writeFunc = opStruct->getOperand(WRITE_IDX);
            assert(isa<Function>(writeFunc) && "write function is not a func");
            res.insert(cast<Function>(writeFunc));
        }
    }
    return res;
}

void
VBoxIndirectCallAnalyzer::analyze(std::vector<Function*> writeSims) {
    std::unordered_set<Function*> visited;
    for (Function* func : writeSims) {
        visited.insert(func);
        checkCalls(func, visited);
    }
}

void
VBoxIndirectCallAnalyzer::checkCalls(Function* func, std::unordered_set<Function*>& visited) {
    for (auto bbItr = func->begin(); bbItr != func->end(); ++bbItr) {
        for (auto insItr = bbItr->begin(); insItr != bbItr->end(); ++insItr) {
            Instruction* ins = &(*insItr);
            if (isa<CallInst>(ins) || isa<InvokeInst>(ins)) {
                CallBase* call = cast<CallBase>(ins);
                if (call->isIndirectCall()) {
                    // Indirect call -> check target
                    Value* callee = call->getCalledOperand();
                    assert(isa<LoadInst>(callee) && "callee is not a load");
                    LoadInst* load = cast<LoadInst>(callee);
                    Value* pointer = load->getPointerOperand();
                    std::vector<Value*> candidates;
                    if (isa<PHINode>(pointer)) {
                        PHINode* phi = cast<PHINode>(pointer);
                        for (unsigned i = 0; i < phi->getNumIncomingValues(); ++i) {
                            Value* incoming = phi->getIncomingValue(i);
                            assert(isa<GetElementPtrInst>(incoming) && "incoming is not a gep");
                            candidates.push_back(incoming);
                        }
                    } else {
                        candidates.push_back(pointer);
                        assert(isa<GetElementPtrInst>(pointer) && "pointer is not a gep");
                    }
                    for (Value* candidate : candidates) {
                        GetElementPtrInst* gep = cast<GetElementPtrInst>(candidate);
                        Value* source = gep->getPointerOperand();
                        if (isa<LoadInst>(source)) {
                            load = cast<LoadInst>(source);
                            pointer = load->getPointerOperand();
                            for (auto lIt = pointer->user_begin(); lIt != pointer->user_end(); ++lIt) {
                                if (isa<StoreInst>(*lIt)) {
                                    StoreInst* store = cast<StoreInst>(*lIt);
                                    Value* v = store->getValueOperand();
                                    if (isa<GetElementPtrInst>(v)) {
                                        gep = cast<GetElementPtrInst>(v);
                                        source = gep->getPointerOperand();
                                        if (isa<GlobalVariable>(source)) {
                                            GlobalVariable* GV = cast<GlobalVariable>(source);
                                            callMap[call].insert(GV);
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    // Direct call -> inter-procedural
                    Function* callee = call->getCalledFunction();
                    if (callee == NULL || callee->isDeclaration() || callee->isIntrinsic() || visited.count(callee) > 0) {
                        continue;
                    }
                    visited.insert(callee);
                    checkCalls(callee, visited);
                }   
            }
        }
    }
}