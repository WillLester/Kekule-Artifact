#include "DeviceBasicAnalyzer.hpp"
using namespace llvm;

#define OBJECT_IDX 0
#define ALIGN_IDX 0
#define NAME_IDX 0
#define PARENT_IDX 1
#define WRITE_IDX 1
#define INSTANCE_INIT_IDX 4
#define CLASS_INIT_IDX 9
#define CLASS_ARG_IDX 0
#define SIM_STRUCT_IDX 2
#define DEVICE_CLASS_NAME_IDX 4
#define DEVICE_CLASS_REALIZE_IDX 8
#define PCI_DEVICE_CLASS_REALIZE_IDX 1

class QemuBasicAnalyzer : public DeviceBasicAnalyzer {
public:
    std::unordered_set<std::string> getDevStructTypeNames(Module& M) override;
    std::vector<Function*> findWriteSimulations(Module& M, std::unordered_set<std::string> stateNames) override;
    std::vector<GlobalVariable*> findTypeInfo(Module& M) override;
private:
    Constant* getUnalignedGVInit(GlobalVariable* GV);
    std::string transGVToStr(GlobalVariable* GV);
    std::unordered_set<Type*> getStateTypes(Module& M);
    Function* findRealize(Value* deviceClass, unsigned realizeIdx);
    Function* getWriteSimFromStruct(GlobalVariable* simGV);
};