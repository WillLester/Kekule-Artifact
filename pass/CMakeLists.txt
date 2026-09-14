add_llvm_pass_plugin(Kekule
	PassEntry.cpp
	DeviceModulePass.cpp
	InstrumentationPass.cpp
	TaintAnalyzer.cpp
	AliasTaintAnalyzer.cpp
	DevStructTaintAnalyzer.cpp
	TypeAnalyzer.cpp
	RegBehaviorAnalyzer.cpp
	DependencyAnalyzer.cpp
	QemuBasicAnalyzer.cpp
	VBoxBasicAnalyzer.cpp
	VBoxIndirectCallAnalyzer.cpp
	QemuIndirectCallAnalyzer.cpp
)


