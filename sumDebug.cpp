#include <iostream>
#include <fstream>
#include <numeric>
#include <vector>

#include <llvm/Support/TargetSelect.h> // InitializeNativeTarget
#include <llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h> // for optimizations

#include <llvm/IR/Verifier.h>

#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>


llvm::ExitOnError ExitOnErr;


llvm::Function *generateFunction(llvm::LLVMContext &Context, llvm::Module &M){
	// emit LLVM IR
	//TODO: add debug information
	llvm::Type *int64_type = llvm::Type::getInt64Ty(Context);
	llvm::Type *int64_ptr_type = llvm::Type::getInt64PtrTy(Context);

	// Signature of the generated function.
	llvm::FunctionType *jit_func_type = llvm::FunctionType::get(int64_type, {int64_ptr_type, int64_type}, false);
	llvm::Function *jit_func = llvm::Function::Create(jit_func_type, llvm::Function::ExternalLinkage, "sumfunc", M);

	llvm::BasicBlock *check_bb = llvm::BasicBlock::Create(Context, "check", jit_func);
	llvm::BasicBlock *loop_bb = llvm::BasicBlock::Create(Context, "loop", jit_func);
	llvm::BasicBlock *exit_bb = llvm::BasicBlock::Create(Context, "exit", jit_func);
	llvm::IRBuilder<> builder(check_bb);
	// function arguments
	llvm::Function::arg_iterator args = jit_func->arg_begin();
	llvm::Value *arr = args++;
	arr->setName("arr");
	llvm::Value *count = args++;
	count->setName("count");

	// declarations
	llvm::Value *addr_sum = builder.CreateAlloca(int64_type, nullptr, "sum");
	llvm::Value *addr_i = builder.CreateAlloca(int64_type, nullptr, "i");

	builder.CreateStore(llvm::ConstantInt::get(int64_type, 0), addr_sum);
	builder.CreateStore(llvm::ConstantInt::get(int64_type, 0), addr_i);

	// if(count == 0)
	llvm::Value *nullcheck = builder.CreateICmpEQ(count, llvm::ConstantInt::get(int64_type, 0), "nullcheck");
	//     goto exit
	builder.CreateCondBr(nullcheck, exit_bb, loop_bb);

	builder.SetInsertPoint(loop_bb);
	// addr = &arr[i], i - index into array
	llvm::Value *addr = builder.CreateGEP(arr, builder.CreateLoad(addr_i), "addr");
	// val = *addr
	llvm::Value *val = builder.CreateLoad(addr, "val");
	// new sum = old sum + val
	builder.CreateStore(
		builder.CreateAdd(
			builder.CreateLoad(addr_sum),
			val
		),
		addr_sum
	);
	// new index = old index + 1
	builder.CreateStore(
		builder.CreateAdd(
			builder.CreateLoad(addr_i),
			llvm::ConstantInt::get(int64_type, 1)
		),
		addr_i
	);
	// if(new index == count)
	llvm::Value *cond = builder.CreateICmpEQ(builder.CreateLoad(addr_i), count, "cond");
	// goto exit else goto loop
	builder.CreateCondBr(cond, exit_bb, loop_bb);

	builder.SetInsertPoint(exit_bb);
	// return value
	llvm::Value *ret = builder.CreateLoad(addr_sum);
	builder.CreateRet(ret);

	return jit_func;
}


bool verifyFunction(llvm::Function *jit_func){
	std::string str;
	llvm::raw_string_ostream os(str);
	bool failed = llvm::verifyFunction(*jit_func, &os);
	if(failed){
		fprintf(stderr, "\nfunction verifier:\n%s\n", os.str().c_str());
	}
	return !failed;
}

void printIR(const llvm::Module &M, const char *fname){
	std::string str;
	llvm::raw_string_ostream os(str);
	M.print(os, nullptr);
	std::ofstream of(fname);
	of << os.str();
}

int main(){
	// initialize LLVM
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();

	using SumFunc = int64_t (*)(const int64_t *arr, size_t count);
	std::unique_ptr<llvm::orc::LLJIT> J;
	SumFunc fn=nullptr;
	{ // scope to test life-cycles

	auto Context = std::make_unique<llvm::LLVMContext>();
	auto M = std::make_unique<llvm::Module>("test", *Context);

	llvm::Function *jit_func = generateFunction(*Context, *M);

	// print
	printIR(*M, "sumDebug.ll");
	// verify
	verifyFunction(jit_func);

#if 1
	int optLevel = 3;

	//TODO: use TransformLayer instead
			llvm::PassManagerBuilder pm_builder;
			pm_builder.OptLevel = optLevel;
			pm_builder.SizeLevel = 0;
			//pm_builder.Inliner = llvm::createAlwaysInlinerLegacyPass();
			pm_builder.Inliner = llvm::createFunctionInliningPass(optLevel, 0, false);
			pm_builder.LoopVectorize = true;
			pm_builder.SLPVectorize = true;

			//pm_builder.VerifyInput = true;
			pm_builder.VerifyOutput = true;

			llvm::legacy::FunctionPassManager function_pm(M.get());
			llvm::legacy::PassManager module_pm;
			pm_builder.populateFunctionPassManager(function_pm);
			pm_builder.populateModulePassManager(module_pm);

			function_pm.doInitialization();
			//for(llvm::Function *f : functions){
			//	function_pm.run(*f);
			//}
			function_pm.run(*jit_func);

			module_pm.run(*M);

	// print
	printIR(*M, "sumDebug_opt.ll");
#endif

	//auto J = ExitOnErr(
	//std::unique_ptr<llvm::orc::LLJIT> J = ExitOnErr(
	J = ExitOnErr(
		llvm::orc::LLJITBuilder()
			.setJITTargetMachineBuilder(cantFail(llvm::orc::JITTargetMachineBuilder::detectHost()))
			.setCompileFunctionCreator([&](llvm::orc::JITTargetMachineBuilder JTMB)
				-> llvm::Expected<std::unique_ptr<llvm::orc::IRCompileLayer::IRCompiler>>
			{
				auto TM = JTMB.createTargetMachine();
				if (!TM) return TM.takeError();
				return std::make_unique<llvm::orc::TMOwningSimpleCompiler>(std::move(*TM));
			})
			.setObjectLinkingLayerCreator([&](llvm::orc::ExecutionSession &ES, const llvm::Triple &TT) {
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

	llvm::orc::ThreadSafeModule tsm(std::move(M), std::move(Context));
	ExitOnErr(J->addIRModule(std::move(tsm)));
	// Look up the JIT'd function
	//auto SumSym = ExitOnErr(J->lookup("sumfunc"));
	llvm::JITEvaluatedSymbol SumSym = ExitOnErr(J->lookup("sumfunc"));
	// get function ptr
	/*SumFunc*/ fn = (SumFunc)SumSym.getAddress();

	} // scope end

	// generate some data
	std::vector<int64_t> data(1 << 25);
	std::iota(data.begin(), data.end(), 0);

	// execute
	int64_t result = fn(data.data(), data.size());
	std::cout << result << '\n';

	return 0;
}
