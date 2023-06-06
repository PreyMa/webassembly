#pragma once

#include <array>

#include "host_module.h"

namespace WASM {
	class FunctionHandle {
	public:
		FunctionHandle(std::string n, Function& f)
			: mName{ std::move(n) }, mFunction{ f } {}
	private:
		friend class Interpreter;

		std::string mName;
		Function& mFunction;
	};

	class Interpreter {
	public:
		Interpreter();
		~Interpreter();

		void loadModule(std::string);
		HostModuleHandle registerHostModule(HostModuleBuilder&);
		void compileAndLinkModules();

		FunctionHandle functionByName(std::string_view, std::string_view);
		
		void runStartFunctions();

		template<typename ...Args>
		ValuePack runFunction(const FunctionHandle& handle, Args... args) {
			std::array<Value, sizeof...(Args)> argumentArray{ Value::fromType<Args>( args )... };
			return executeFunction(handle.mFunction, argumentArray);
		}

		void attachIntrospector(std::unique_ptr<Introspector>);

	private:
		friend class Module;
		friend class HostModule;
		friend class ModuleLinker;
		friend class ModuleCompiler;
		friend class DataItem;

		struct FunctionLookup {
			const Function& function;
			const Module& module;
		};

		void registerModuleName(NonNull<ModuleBase>);

		ValuePack executeFunction(Function&, std::span<Value>);
		ValuePack runInterpreterLoop(const BytecodeFunction&, std::span<Value>);

		Nullable<Function> findFunction(const std::string&, const std::string&);
		ModuleBase& findModule(const std::string&);

		InterpreterTypeIndex indexOfFunctionType(const FunctionType&) const;
		InterpreterFunctionIndex indexOfFunction(const BytecodeFunction&) const;
		InterpreterMemoryIndex indexOfMemoryInstance(const Memory&) const;
		InterpreterTableIndex indexOfTableInstance(const FunctionTable&);
		InterpreterLinkedElementIndex indexOfLinkedElement(const LinkedElement&);
		InterpreterLinkedDataIndex indexOfLinkedDataItem(const LinkedDataItem&);

		void initState(const BytecodeFunction& function);
		void saveState(const u8*, u32*, u32*, Memory*);
		void dumpStack(std::ostream&) const;
		std::optional<FunctionLookup> findFunctionByBytecodePointer(const u8*) const;

		std::list<Module> wasmModules;
		std::list<HostModule> hostModules;
		std::unordered_map<std::string, NonNull<ModuleBase>> moduleNameMap;
		SealedVector<FunctionType> allFunctionTypes;
		SealedVector<BytecodeFunction> allFunctions;
		SealedVector<FunctionTable> allTables;
		SealedVector<Memory> allMemories;
		SealedVector<Global<u32>> allGlobals32;
		SealedVector<Global<u64>> allGlobals64;
		SealedVector<LinkedElement> allElements;
		SealedVector<LinkedDataItem> allDataItems;

		bool hasLinkedAndCompiled{ false };
		bool isInterpreting{ false };
		std::unique_ptr<u32[]> mStackBase;
		u32* mStackPointer{ nullptr };
		u32* mFramePointer{ nullptr };
		Memory* mMemoryPointer{ nullptr };
		const u8* mInstructionPointer{ nullptr };

		std::unique_ptr<Introspector> attachedIntrospector;
	};
}