#include "IndirectCallAnalyzer.hpp"

class QemuIndirectCallAnalyzer : public IndirectCallAnalyzer {
public:
    QemuIndirectCallAnalyzer() {};
    void analyze(std::vector<Function*> writeSims) override;
    std::unordered_set<Function*> getTarget(CallBase* call) override;
};