#pragma once

#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/GlobalVariable.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>



using namespace llvm;

class DeviceBasicAnalyzer {
public:
	virtual std::vector<GlobalVariable*> findTypeInfo(Module& M) = 0;
	virtual std::unordered_set<std::string> getDevStructTypeNames(Module& M) = 0;
	virtual std::vector<Function*> findWriteSimulations(Module& M, std::unordered_set<std::string> stateNames) = 0;
};
