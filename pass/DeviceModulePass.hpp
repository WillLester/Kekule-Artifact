#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "DeviceBasicAnalyzer.hpp"
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

using namespace llvm;

class DeviceModulePass : public PassInfoMixin<DeviceModulePass> {
public:
	PreservedAnalyses run(Module& M, ModuleAnalysisManager& AM);
private:
	bool isCoreFile(Module& M);
	bool isFunctionFiltered(std::string funcName);
	Function* findRealize(Value* deviceClass, unsigned realizeIdx);
	bool findWriteSimulations(Module& M);
	std::vector<std::string> getLLsByHeader(StringRef headerName);
	std::string transferHeaderToLL(StringRef headerName);
	std::vector<StringRef> getModuleFilesThroughArg(Function* func, std::unordered_set<Function*>& visited, unsigned idx);
	std::unordered_set<std::string> getModuleFiles(Module& M);
	void writeScript(const std::string moduleName, std::unordered_set<std::string> relatedFiles);
	DeviceBasicAnalyzer* basicAnalyzer;
	std::vector<GlobalVariable*> typeInfos;
	std::vector<Function*> writeSims;
	std::string libCommon;
	std::unordered_set<std::string> filteredFuncs = {"qemu_log","object_dynamic_cast_assert","memcmp","object_get_class","strcmp","AroundInvalidAddress"};
	std::unordered_set<std::string> filteredPrefix = {"__ubsan","__asan","__sanitizer", "__fprintf","qemu_", "g_", "blk_",
													  "memory_region","address_space","event_notifier","timer_","qdev","qbus",
													  "notifier","object_","pixman_","dpy_","xen_","msix","device_","trace_",
													  "dma_","iov_", "spice_qxl_","cpu_physical","scsi_cdb", "sysbus_"};
	std::unordered_map<std::string, std::vector<std::string>> adhocImpls =
		{{"hw/nvme/nvme.h", {"libcommon.fa.p/hw_nvme_ns.c.ll", "libcommon.fa.p/hw_nvme_subsys.c.ll"}},
		 {"hw/usb.h", {"libcommon.fa.p/hw_usb_core.c.ll", "libcommon.fa.p/hw_usb_bus.c.ll", "libcommon.fa.p/hw_usb_libhw.c.ll"}},
		 {"hw/scsi/scsi.h", {"libcommon.fa.p/hw_scsi_scsi-bus.c.ll"}},
		 {"hw/sd/sdhci-internal.h", {"libcommon.fa.p/hw_sd_sdhci.c.ll"}},
		 {"hw/sd/sd.h", {"libcommon.fa.p/hw_sd_core.c.ll"}},
		 {"hw/display/vga_int.h", {"libcommon.fa.p/hw_display_vga.c.ll"}},
		 {"qemu/fifo8.h", {"libqemuutil.a.p/util_fifo8.c.ll"}},
		 {"qemu/bitops.h", {"libqemuutil.a.p/util_bitops.c.ll"}},
		 {"hw/display/ati_int.h", {"libcommon.fa.p/hw_display_ati_2d.c.ll", "libcommon.fa.p/hw_display_ati_dbg.c.ll"}},
		 {"hw/i2c/i2c.h", {"libcommon.fa.p/hw_i2c_core.c.ll"}},
		 {"net/can_emu.h", {"libcommon.fa.p/net_can_can_core.c.ll"}},
		 {"hw/ptimer.h", {"libcommon.fa.p/hw_core_ptimer.c.ll"}},
		 {"hw/ide/ide-internal.h", {"libcommon.fa.p/hw_ide_core.c.ll"}},
		 {"hw/block/fdc-internal.h", {"libcommon.fa.p/hw_block_fdc.c.ll"}},
		 {"hw/display/vga_int.h", {"libcommon.fa.p/hw_display_vga.c.ll"}},
		 {"hw/display/qxl.h", {"libcommon.fa.p/hw_display_qxl.c.ll", "libcommon.fa.p/hw_display_qxl-render.c.ll"}}};
};
