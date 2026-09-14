#include "DeviceModulePass.hpp"
#include "QemuBasicAnalyzer.hpp"
#include "TaintAnalyzer.hpp"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <unordered_set>

using namespace llvm;

cl::opt<std::string> qemuBuildPath("qemu_build_path",
	cl::init(""),
	cl::desc("The path to QEMU build."),
	cl::value_desc("The path to QEMU build."));

cl::opt<std::string> llvmLibPath("llvm_lib_path",
	cl::init(""),
	cl::desc("The path to the built LLVM shared lib."),
	cl::value_desc("The path to built LLVM shared lib."));

PreservedAnalyses
DeviceModulePass::run(Module& M, ModuleAnalysisManager& AM) {
	basicAnalyzer = new QemuBasicAnalyzer();
	if (!isCoreFile(M)) {
		errs() << "Not a core file\n";
		return PreservedAnalyses::all();
	}
	std::string moduleName = M.getModuleIdentifier().substr(2);
	size_t pos = moduleName.find('/');
	libCommon = moduleName.substr(0, pos);
	std::unordered_set<std::string> files = getModuleFiles(M);
	writeScript(moduleName, files);
	return PreservedAnalyses::all();
}

bool
DeviceModulePass::isCoreFile(Module& M) {
	typeInfos = basicAnalyzer->findTypeInfo(M);
	return !typeInfos.empty();
}

bool
DeviceModulePass::isFunctionFiltered(std::string funcName) {
	if (filteredFuncs.count(funcName) > 0) {
		return true;
	}
	for (std::string prefix : filteredPrefix) {
		if (funcName.rfind(prefix, 0) == 0) {
			return true;
		}
	}
	return false;
}

std::vector<std::string>
DeviceModulePass::getLLsByHeader(StringRef headerName) {
	std::vector<std::string> res;
	StringRef noInclude;
	errs() << "include " << headerName << "\n";
	if (headerName.startswith("include") || headerName.startswith("..")) {
		noInclude = headerName.split('/').second;
	} else {
		noInclude = headerName;
	}
	if (adhocImpls.count(noInclude.str()) > 0) {
		for (std::string impl : adhocImpls[noInclude.str()]) {
			size_t pos = impl.find('/');
			std::string dir = impl.substr(0, pos);
			if (dir == "libcommon.fa.p" && libCommon != "libcommon.fa.p") {
				impl.replace(0, 14, libCommon);
			}
			res.push_back(impl);
		}
	} else {
		res.push_back(transferHeaderToLL(noInclude));
	}
	return res;
}

std::string
DeviceModulePass::transferHeaderToLL(StringRef headerName) {
	headerName.consume_back(".h");
	std::string headerStr = headerName.str();
	size_t found = headerStr.find('/');
	while (found != std::string::npos) {
		headerStr[found] = '_';
		found = headerStr.find('/', found + 1);
	}
	return libCommon + "/" + headerStr + ".c.ll";
}

std::vector<StringRef>
DeviceModulePass::getModuleFilesThroughArg(Function* func, std::unordered_set<Function*>& visited, unsigned idx) {
	std::vector<StringRef> result;
	if (visited.count(func) > 0) {
		return result;
	}
	visited.insert(func);
	TaintAnalyzer analyzer(func, func->getArg(idx));
	analyzer.analyze();
	std::unordered_set<Value*> stateVars = analyzer.getTaintedValues();
	for (Value* stateVar : stateVars) {
		for (Value::user_iterator i = stateVar->user_begin(); i != stateVar->user_end(); ++i) {
			if (isa<CallInst>(*i)) {
				CallInst* call = cast<CallInst>(*i);
				if (!call->isIndirectCall() && !call->isInlineAsm()) {
					Function* callee = call->getCalledFunction();
					if (callee->isIntrinsic() || callee->isVarArg()) {
						continue;
					}
					std::string name = callee->getName().str();
					if (isFunctionFiltered(name)) {
						continue;
					}
					if (callee->isDeclaration()) {
					    errs() << "callee " << name << "\n";
						DISubprogram* subprogram = callee->getSubprogram();
						if (subprogram != NULL) {
							StringRef fileName = subprogram->getFilename();
							assert(fileName.endswith(".h") && "header is not in include\n");
							result.push_back(fileName);
						}
					} else {
						for (unsigned j = 0; j < call->arg_size(); ++j) {
							Value* argItr = call->getArgOperand(j);
							if (argItr && stateVars.count(argItr) > 0) {
								std::vector<StringRef> nextLvRes = getModuleFilesThroughArg(callee, visited, j);
								result.insert(result.end(), nextLvRes.begin(), nextLvRes.end());
							}
						}
					}
				}
			}
		}
	}
	return result;
}

std::unordered_set<std::string>
DeviceModulePass::getModuleFiles(Module& M) {
	assert(!typeInfos.empty() && "The entry structure not found!");
	std::unordered_set<std::string> stateNames = basicAnalyzer->getDevStructTypeNames(M);
	std::unordered_set<std::string> result;
	for (Module::iterator i = M.begin(); i != M.end(); ++i) {
		if (!i->isIntrinsic() && i->isDeclaration()) {
			if (i->hasName()) {
				if (isFunctionFiltered(i->getName().str())) {
					continue;
				}
			}
			DISubprogram* subprogram = i->getSubprogram();
			if (subprogram != NULL) {
				DISubroutineType* subroutineTy = subprogram->getType();
				DITypeRefArray types = subroutineTy->getTypeArray();
				for (DITypeRefArray::iterator ty = ++types.begin(); ty != types.end(); ++ty) {
					DIType* tyPtr = *ty;
					while (tyPtr != NULL && isa<DIDerivedType>(tyPtr)) {
						DIDerivedType* derived = cast<DIDerivedType>(tyPtr);
						tyPtr = derived->getBaseType();
					}
					if (tyPtr != NULL && isa<DICompositeType>(tyPtr)) {
						StringRef name = tyPtr->getName();
						if (stateNames.count(name.str()) > 0) {
							if (i->hasName()) {
								errs() << "Function " << i->getName() << "\n";
							}
							StringRef fileName = subprogram->getFilename();
							std::cout << "From subprogram " << fileName.str() << std::endl;
							std::vector<std::string> lls = getLLsByHeader(fileName);
							result.insert(lls.begin(), lls.end());
						}
					}
				}
			}
		}
	}
	writeSims = basicAnalyzer->findWriteSimulations(M, stateNames);
	std::unordered_set<Function*> visited;
	if (writeSims.size() == 0) {
		errs() << "No write sim found\n";
	}
	for (Function* writeSim : writeSims) {
		errs() << "writeSim " << writeSim->getName() << "\n";
		std::vector<StringRef> headers = getModuleFilesThroughArg(writeSim, visited, 0);
		for (StringRef header : headers) {
			std::vector<std::string> lls = getLLsByHeader(header);
			result.insert(lls.begin(), lls.end());
		}
	}
	return result;
}

void
DeviceModulePass::writeScript(const std::string moduleName, std::unordered_set<std::string> relatedFiles) {
	std::string scriptPath = qemuBuildPath + "/device-module.sh";
	std::string linkedPath = qemuBuildPath + "/linked-files.txt";
	std::string relatedPath = qemuBuildPath + "/related-files.txt";
	std::ofstream scriptFile(scriptPath);
	std::string moduleNoPrefix = moduleName;
	std::ifstream linkedFile(linkedPath);
	std::unordered_set<std::string> linkedNames;
	if (linkedFile.is_open()) {
		std::string line;
		while (std::getline(linkedFile, line)) {
			linkedNames.insert(line);
		}
		linkedFile.close();
	}
	linkedNames.insert(moduleNoPrefix);
	std::unordered_set<std::string> filteredRelated;
	for (std::string related : relatedFiles) {
		if (linkedNames.count(related) == 0)
			filteredRelated.insert(related);
	}
	std::ofstream linkedFileOut(linkedPath, std::ios::trunc);
	std::ofstream relatedFile(relatedPath, std::ios::app);
	if (scriptFile.is_open() && linkedFileOut.is_open() && relatedFile.is_open()) {
		if (filteredRelated.empty()) {
			scriptFile << "exit 1" << std::endl;
		} else {
			scriptFile << "#!/bin/sh" << std::endl;
			scriptFile << "set -x" << std::endl;
			scriptFile << "cd " << qemuBuildPath << std::endl;
			for (std::string related : filteredRelated) {
				errs() << "linked file " << related << "\n";
				scriptFile << "make " << related << std::endl;
			}
			scriptFile << "llvm-link -S " << moduleName;
			for (std::string related : filteredRelated) {
				scriptFile << " " << related;
				relatedFile << related << std::endl;
			}
			scriptFile << " -o " << moduleName << std::endl;
			scriptFile << "exit 0" << std::endl;
			scriptFile.close();
		}
		for (std::string related : filteredRelated) {
			linkedNames.insert(related);
		}
		for (std::string name : linkedNames) {
			linkedFileOut << name << std::endl;
		}
		linkedFileOut.close();
	}
}

