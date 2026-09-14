#include "DeviceBasicAnalyzer.hpp"
using namespace llvm;

class VBoxBasicAnalyzer : public DeviceBasicAnalyzer {
public:
    std::vector<GlobalVariable*> findTypeInfo(Module& M) override;
    std::unordered_set<std::string> getDevStructTypeNames(Module& M) override;
    std::vector<Function*> findWriteSimulations(Module& M, std::unordered_set<std::string> stateNames) override;
};