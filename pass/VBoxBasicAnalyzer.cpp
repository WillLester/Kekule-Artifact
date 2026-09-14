#include "VBoxBasicAnalyzer.hpp"
#include "DevStructTaintAnalyzer.hpp"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/InstIterator.h"


#define CONSTRUCT_IDX 15
#define DEVINS_IDX 0
#define DEVICE_IDX 7
#define PIO_WRITE_IDX 3
#define MMIO_WRITE_IDX 4
#define MMIO_EX_WRITE_IDX 6
#define INNER_WRITE_DIX 5

std::vector<GlobalVariable*> VBoxBasicAnalyzer::findTypeInfo(Module& M) {
    std::vector<GlobalVariable*> typeInfos;
    for (llvm::GlobalVariable &GV : M.globals()) {
        if (MDNode* dbgInfo = GV.getMetadata(LLVMContext::MD_dbg)) {
            if (DIGlobalVariableExpression* DIExpr = dyn_cast<DIGlobalVariableExpression>(dbgInfo)) {
                DIGlobalVariable* DIGV = DIExpr->getVariable();
                DIType* DIType = DIGV->getType();
                while (isa<DIDerivedType>(DIType)) {
                    DIDerivedType* derived = cast<DIDerivedType>(DIType);
                    DIType = derived->getBaseType();
                }
                StringRef name = DIType->getName();
                if (name.startswith("PDMDEVREG")) {
                    typeInfos.push_back(&GV);
                }
            }
        }
    }
    return typeInfos;
}

std::unordered_set<std::string> VBoxBasicAnalyzer::getDevStructTypeNames(Module& M) {
    std::vector<GlobalVariable*> typeInfos = findTypeInfo(M);
    std::unordered_set<std::string> stateTypes;
    // Get the construct functions
    for (GlobalVariable* GV : typeInfos) {
        Constant* init = GV->getInitializer();
        if (init->getNumOperands() <= CONSTRUCT_IDX) {
            init = cast<Constant>(init->getOperand(0));
        }
        Value* constructVal = init->getOperand(CONSTRUCT_IDX);
        assert(isa<Function>(constructVal) && "construct is not a function!\n");
        Function* construct = cast<Function>(constructVal);
        Argument* devIns = construct->getArg(DEVINS_IDX);
        DevStructTaintAnalyzer analyzer(construct, devIns);
        analyzer.analyze();
        std::unordered_set<Value*> aliases = analyzer.getAliases();
        for (Value* alias : aliases) {
            for (Argument::user_iterator i = alias->user_begin(); i != alias->user_end(); ++i) {
                if (isa<GetElementPtrInst>(*i)) {
                    GetElementPtrInst* gep = cast<GetElementPtrInst>(*i);
                    if (gep->getNumIndices() == 2) {
                        Value* secondIdx = *(gep->idx_begin() + 1);
                        if (isa<ConstantInt>(secondIdx)) {
                            ConstantInt* CI = cast<ConstantInt>(secondIdx);
                            if (CI->getSExtValue() == DEVICE_IDX) {
                                // The device struct, try to find the load
                                for (auto gIt = gep->user_begin(); gIt != gep->user_end(); ++gIt) {
                                    if (isa<LoadInst>(*gIt)) {
                                        LoadInst* load = cast<LoadInst>(*gIt);
                                        // Finally, get the struct name through the dbg declare
                                        for (auto lIt = load->user_begin(); lIt != load->user_end(); ++lIt) {
                                            if (isa<StoreInst>(*lIt)) {
                                                StoreInst* store = cast<StoreInst>(*lIt);
                                                Value* pointer = store->getPointerOperand();
                                                for (auto &I : instructions(construct)) {
                                                    if (auto *DDI = dyn_cast<DbgDeclareInst>(&I)) {
                                                        if (DDI->getAddress() == pointer) {
                                                            DILocalVariable *var = DDI->getVariable();
                                                            DIType *ty = var->getType();
                                                            while (ty) {
                                                                // Check if it's a derived type
                                                                if (const auto *derived = dyn_cast<DIDerivedType>(ty)) {
                                                                    ty = derived->getBaseType();
                                                                } else {
                                                                    break;
                                                                }
                                                            }
                                                            if (ty) {
                                                                stateTypes.insert(ty->getName().str());
                                                                errs() << "device struct " << ty->getName() << "\n";
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
    }
    return stateTypes;
}

std::vector<Function*> VBoxBasicAnalyzer::findWriteSimulations(Module& M, std::unordered_set<std::string> stateNames) {
    std::vector<GlobalVariable*> typeInfos = findTypeInfo(M);
    std::unordered_set<Function*> writeSimSet;
    // Get the construct functions
    for (GlobalVariable* GV : typeInfos) {
        Constant* init = GV->getInitializer();
        if (init->getNumOperands() <= CONSTRUCT_IDX) {
            init = cast<Constant>(init->getOperand(0));
        }
        Value* constructVal = init->getOperand(CONSTRUCT_IDX);
        assert(isa<Function>(constructVal) && "construct is not a function!");
        Function* construct = cast<Function>(constructVal);
        for (Function::iterator i = construct->begin(); i != construct->end(); ++i) {
            BasicBlock* BB = &(*i);
            for (BasicBlock::iterator j = BB->begin(); j != BB->end(); ++j) {
                Instruction* ins = &(*j);
                if (isa<CallInst>(ins)) {
                    CallInst* call = cast<CallInst>(ins);
                    Function* callee = call->getCalledFunction();
                    if (callee != NULL) {
                        StringRef funcName = callee->getName();
                        bool isCreateIO = false;
                        size_t writeIdx = 0;
                        if (funcName.contains("PDMDevHlpPCIIORegionCreateIo")) {
                            writeIdx = PIO_WRITE_IDX;
                            isCreateIO = true;
                        } else if (funcName.contains("PDMDevHlpPCIIORegionCreateMmio")) {
                            writeIdx = MMIO_WRITE_IDX;
                            isCreateIO = true;
                        } else if (funcName.contains("PDMDevHlpIoPortCreateAndMap")) {
                            writeIdx = PIO_WRITE_IDX;
                            isCreateIO = true;
                        } else if (funcName.contains("PDMDevHlpMmioCreateExAndMap")) {
                            writeIdx = MMIO_EX_WRITE_IDX;
                            isCreateIO = true;
                        }
                        if (isCreateIO) {
                            if (writeIdx < call->arg_size()) {
                                Value* writeFunc = call->getArgOperand(writeIdx);
                                assert(isa<Function>(writeFunc) && "write is not a func\n");
                                Function* writeSim = cast<Function>(writeFunc);
                                writeSimSet.insert(writeSim);
                            } else {
                                bool foundCall = false;
                                for (Function::iterator calleeI = callee->begin(); calleeI != callee->end(); ++calleeI) {
                                    if (foundCall) {
                                        break;
                                    }
                                    for (BasicBlock::iterator calleeJ = calleeI->begin(); calleeJ != calleeI->end(); ++calleeJ) {
                                        Instruction* innerIns = &(*calleeJ);
                                        if (isa<CallInst>(innerIns)) {
                                            CallInst* innerCall = cast<CallInst>(innerIns);
                                            if (!innerCall->isIndirectCall()) {
                                                continue;
                                            }
                                            // Should be the nested call
                                            Value* writeFunc = innerCall->getArgOperand(INNER_WRITE_DIX);
                                            assert(isa<Function>(writeFunc) && "nested write is not a func\n");
                                            Function* writeSim = cast<Function>(writeFunc);
                                            writeSimSet.insert(writeSim);
                                            foundCall = true;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    std::vector<Function*> writeSims(writeSimSet.begin(), writeSimSet.end());
    return writeSims;
}
