#include "DeviceModulePass.hpp"
#include "InstrumentationPass.hpp"

/* New PM Registration */
PassPluginLibraryInfo
getKekulePluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "Kekule", LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
            [](StringRef Name, ModulePassManager &PM,
            ArrayRef<PassBuilder::PipelineElement>) {
            if (Name == "devicemodule") {
                PM.addPass(DeviceModulePass());
                return true;
            }
            if (Name == "instrumentation") {
                PM.addPass(InstrumentationPass());
                return true;
            }
            return false;
        });
    }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getKekulePluginInfo();
}
