#include "IndirectCallAnalyzer.hpp"

class VBoxIndirectCallAnalyzer : public IndirectCallAnalyzer {
public:
    VBoxIndirectCallAnalyzer() {};
    void analyze(std::vector<Function*> writeSims) override;
    std::unordered_set<Function*> getTarget(CallBase* call) override;
private:
    void checkCalls(Function* func, std::unordered_set<Function*>& visited);
};