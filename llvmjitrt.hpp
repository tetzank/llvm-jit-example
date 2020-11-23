#include <memory> // unique_ptr

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>


class llvmjitrt {
private:
	//std::unique_ptr<llvm::orc::LLJIT> J;

public:
	//FIXME: make private, add additional methods for missing interactions
	std::unique_ptr<llvm::orc::LLJIT> J;

	//TODO: add configuration parameters/flags
	// like dump_assembly, gdb_support, perf_support, opt_level
	llvmjitrt(){
		//J = ExitOnErr(
		J = cantFail(
			llvm::orc::LLJITBuilder()
				.setJITTargetMachineBuilder(cantFail(llvm::orc::JITTargetMachineBuilder::detectHost()))
				.setCompileFunctionCreator([&](llvm::orc::JITTargetMachineBuilder JTMB)
					-> llvm::Expected<std::unique_ptr<llvm::orc::IRCompileLayer::IRCompiler>>
				{
					auto TM = JTMB.createTargetMachine();
					if (!TM) return TM.takeError();
					return std::make_unique<llvm::orc::TMOwningSimpleCompiler>(std::move(*TM));
				})
				.setObjectLinkingLayerCreator([&](llvm::orc::ExecutionSession &ES, const llvm::Triple &/*TT*/) {
					auto GetMemMgr = []() {
						return std::make_unique<llvm::SectionMemoryManager>();
					};
					auto ObjLinkingLayer = std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(
						ES, std::move(GetMemMgr)
					);
					ObjLinkingLayer->registerJITEventListener(
						*llvm::JITEventListener::createGDBRegistrationListener()
					);
					llvm::JITEventListener *perfListener = llvm::JITEventListener::createPerfJITEventListener();
					// is nullptr without LLVM_USE_PERF when compiling LLVM
					assert(perfListener);
					ObjLinkingLayer->registerJITEventListener(*perfListener);
					return ObjLinkingLayer;
				})
				.create()
		);
		// dumps object files to current working directory (directory can be changed by parameter)
		J->getObjTransformLayer().setTransform(llvm::orc::DumpObjects());
	}
};
