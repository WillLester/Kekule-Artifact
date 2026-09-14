#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include <string>
#include <unordered_set>
#include <unordered_map>

#pragma once

using namespace llvm;

class IndirectCallAnalyzer {
public:
    IndirectCallAnalyzer() {};
    virtual void analyze(std::vector<Function*> writeSims) = 0;
    virtual std::unordered_set<Function*> getTarget(CallBase* call) = 0;
protected:
    std::unordered_map<CallBase*, std::unordered_set<GlobalVariable*>> callMap;
};