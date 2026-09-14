#include "QemuBasicAnalyzer.hpp"
#include "DevStructTaintAnalyzer.hpp"

Constant* QemuBasicAnalyzer::getUnalignedGVInit(GlobalVariable* GV) {
    Constant* GVInit = GV->getInitializer();
    return cast<Constant>(GVInit->getOperand(ALIGN_IDX));
}

std::vector<GlobalVariable*> QemuBasicAnalyzer::findTypeInfo(Module& M) {
    std::vector<GlobalVariable*> typeInfos;
    for (Module::global_iterator i = M.global_begin(); i != M.global_end(); ++i) {
        Type* valueTy = i->getValueType();
        if (isa<ArrayType>(valueTy)) {
            ArrayType* arrayTy = cast<ArrayType>(valueTy);
            valueTy = arrayTy->getElementType();
        }
        if (isa<StructType>(valueTy)) {
            StructType* valueStructTy = cast<StructType>(valueTy);
            if (valueStructTy->isOpaque()) {
                continue;
            }
            Type* eleTy = valueStructTy->getElementType(0);
            if (isa<ArrayType>(eleTy)) {
                ArrayType* arrayTy = cast<ArrayType>(eleTy);
                eleTy = arrayTy->getElementType();
            }
            if (isa<StructType>(eleTy)) {
                StructType* eleStructTy = cast<StructType>(eleTy);
                if (eleStructTy->hasName() && eleStructTy->getName().equals("struct.TypeInfo")) {
                    errs() << "find type info in ele\n";
                    typeInfos.push_back(&(*i));
                }
            }
            if (valueStructTy->hasName() && valueStructTy->getName().equals("struct.TypeInfo")) {
                errs() << "find type info\n";
                typeInfos.push_back(&(*i));
            }
        }
    }
    return typeInfos;
}

std::string QemuBasicAnalyzer::transGVToStr(GlobalVariable* GV) {
    Constant* GVInit = GV->getInitializer();
    if (!isa<ConstantDataArray>(GVInit)) {
        GVInit = getUnalignedGVInit(GV);
    }
    assert(isa<ConstantDataArray>(GVInit) && "not a constant data array\n");
    ConstantDataArray* arr = cast<ConstantDataArray>(GVInit);
    assert(arr->isString() && "not a string\n");
    return arr->getAsString().drop_back().str();
}

std::unordered_set<Type*> QemuBasicAnalyzer::getStateTypes(Module& M) {
    std::vector<std::string> noStateStruct = {"PCIDevice", "PCIHostState", "DeviceState","PCIBus","BusState","NvmeNamespace","NvmeSubsystem","eeprom93xx_new","eeprom93xx_free"};
    std::unordered_set<Type*> states;
    for (Module::iterator func = M.begin(); func != M.end(); ++func) {
        if (func->isDeclaration() || func->isIntrinsic() || func->arg_size() == 0) {
            continue;
        }
        Argument* obj = func->getArg(OBJECT_IDX);
        DevStructTaintAnalyzer analyzer(&(*func), obj);
        analyzer.analyze();
        std::unordered_set<Value*> aliases = analyzer.getAliases();
        for (Value* alias : aliases) {
            for (Value::user_iterator i = alias->user_begin(); i != alias->user_end(); ++i) {
                if (isa<GetElementPtrInst>(*i)) {
                    GetElementPtrInst* gep = cast<GetElementPtrInst>(*i);
                    Type* sourceTy = gep->getSourceElementType();
                    if (!sourceTy->isStructTy()) {
                        continue;
                    }
                    bool notStateStruct = false;
                    for (std::string noStruct : noStateStruct) {
                        if (sourceTy->getStructName().endswith("." + noStruct)) {
                            notStateStruct = true;
                            break;
                        }
                    }
                    if (notStateStruct) {
                        continue;
                    }
                    if (analyzer.hasObjectCastCall() || sourceTy->getStructName().endswith("State")) {
                        states.insert(sourceTy);
                    } else {
                        continue;
                    }
                    // The state may include sub states.
                    Type* elementTy = gep->getResultElementType();
                    if (elementTy->isStructTy() &&
                        (elementTy->getStructName().endswith("State") || elementTy->getStructName().endswith("State_st")
                        || elementTy->getStructName().endswith("FDCtrl"))) {
                        states.insert(elementTy);
                        StructType* structTy = cast<StructType>(elementTy);
                        Type* first = structTy->getElementType(0);
                        if (first->isStructTy() && first->getStructName().endswith("State")) {
                            bool firstNotState = false;
                            for (std::string noStruct : noStateStruct) {
                                if (first->getStructName().endswith("." + noStruct)) {
                                    firstNotState = true;
                                    break;
                                }
                            }
                            if (!firstNotState) {
                                errs() << "First member state\n";
                                first->print(errs());
                                errs() << "\n";
                                states.insert(first);
                            }
                        }
                    }
                }	
            }
        }
    }
    return states;
}

std::unordered_set<std::string> QemuBasicAnalyzer::getDevStructTypeNames(Module& M) {
    std::unordered_set<Type*> stateTypes = getStateTypes(M);
    std::unordered_set<std::string> stateNames;
    for (Type* stateType : stateTypes) {
        StringRef name = stateType->getStructName();
        errs() << "State " << name << "\n";
        stateNames.insert(name.rsplit('.').second.str());
    }
    return stateNames;
}

/**
    Find the realize functions.
    Arg:
        deviceClass: the casted device class object
        realizeIdx: the index of realize function in the device class object
    Return:
        the pointer to the realize function, nullptr if nothing detected
    **/
Function* QemuBasicAnalyzer::findRealize(Value* deviceClass, unsigned realizeIdx) {
    for (Value::user_iterator i = deviceClass->user_begin(); i != deviceClass->user_end(); ++i) {
        if (isa<GetElementPtrInst>(*i)) {
            GetElementPtrInst* gep = cast<GetElementPtrInst>(*i);
            if (gep->getNumIndices() == 2) {
                GetElementPtrInst::op_iterator idx = gep->idx_begin() + 1;
                assert(idx != gep->idx_end() && "Realize idx not found");
                if (isa<ConstantInt>(*idx)) {
                    ConstantInt* idxInt = cast<ConstantInt>(*idx);
                    if (idxInt->equalsInt(realizeIdx)) {
                        for (Value::user_iterator j = gep->user_begin(); j != gep->user_end(); ++j) {
                            if (isa<StoreInst>(*j)) {
                                StoreInst* store = cast<StoreInst>(*j);
                                Value* storedValue = store->getValueOperand();
                                if (isa<Function>(storedValue)) {
                                    Function* realizeFunc = cast<Function>(storedValue);
                                    return realizeFunc;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}
	
/**
 *  Get the write simulation function from a struct.
 */
Function* QemuBasicAnalyzer::getWriteSimFromStruct(GlobalVariable* simGV) {
    Constant* simGVInit = simGV->getInitializer();
    if (!isa<Function>(simGVInit->getOperand(WRITE_IDX))) {
        simGVInit = getUnalignedGVInit(simGV);
    }
    assert(isa<ConstantStruct>(simGVInit) && "Simulation struct is not a struct");
    ConstantStruct* simGVStruct = cast<ConstantStruct>(simGVInit);
    Value* writeOp = simGVStruct->getOperand(WRITE_IDX);
    assert(isa<Function>(writeOp));
    Function* writeSim = cast<Function>(writeOp);
    errs() << "Find write sims\n";
    return writeSim;
}

/**
    Detect the write simulation functions.
    **/
std::vector<Function*> QemuBasicAnalyzer::findWriteSimulations(Module& M, std::unordered_set<std::string> stateNames) {
    std::unordered_map<std::string, std::string> simMap = {{"SDHCIState", "sdhci_write"}};
    std::vector<Function*> writeSims;
    for (Module::iterator func = M.begin(); func != M.end(); ++func) {
        for (Function::iterator bb = func->begin(); bb != func->end(); ++bb) {
            for (BasicBlock::iterator ins = bb->begin(); ins != bb->end(); ++ins) {
                if (isa<CallInst>(ins)) {
                    CallInst* call = cast<CallInst>(ins);
                    Function* callee = call->getCalledFunction();
                    if (callee != nullptr && callee->getName().equals("memory_region_init_io")) {
                        Value* simStruct = call->getArgOperand(SIM_STRUCT_IDX);
                        if (!isa<GlobalVariable>(simStruct)) {
                            bool foundSim = false;
                            for (std::string stateName : stateNames) {
                                if (simMap.count(stateName) > 0) {
                                    Function* writeSim = M.getFunction(simMap[stateName]);
                                    if (writeSim != NULL) {
                                        writeSims.push_back(writeSim);
                                        foundSim = true;
                                        break;
                                    }
                                }
                            }
                            if (isa<SelectInst>(simStruct)) {
                                SelectInst* select = cast<SelectInst>(simStruct);
                                Value* trueVal = select->getTrueValue();
                                if (isa<GlobalVariable>(trueVal)) {
                                    GlobalVariable* simGV = cast<GlobalVariable>(trueVal);
                                    Function* writeSim = getWriteSimFromStruct(simGV);
                                    if (writeSim != NULL) {
                                        writeSims.push_back(writeSim);
                                    }
                                    foundSim = true;
                                }
                                Value* falseVal = select->getFalseValue();
                                if (isa<GlobalVariable>(falseVal)) {
                                    GlobalVariable* simGV = cast<GlobalVariable>(falseVal);
                                    Function* writeSim = getWriteSimFromStruct(simGV);
                                    if (writeSim != NULL) {
                                        writeSims.push_back(writeSim);
                                    }
                                    foundSim = true;
                                }
                                if (!foundSim) {
                                    assert(false && "No sim found in select\n");
                                }
                            }
                            if (foundSim) {
                                continue;
                            }
                        }
                        if (!isa<GlobalVariable>(simStruct)) {
                            simStruct->print(errs());
                            errs() << "\n";
                        }
                        assert(isa<GlobalVariable>(simStruct) && "Simulation struct is not global");
                        GlobalVariable* simGV = cast<GlobalVariable>(simStruct);
                        if (simGV->hasExternalLinkage()) {
                            continue;
                        }
                        Function* writeSim = getWriteSimFromStruct(simGV);
                        if (writeSim != NULL) {
                            writeSims.push_back(writeSim);
                        } else {
                            assert(false && "No write sim found in the struct\n");
                        }
                    }
                }
            }
        }
    }
    return writeSims;
}