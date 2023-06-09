
#pragma once

#include <span>
#include <functional>

#include "decoding.h"
#include "bytecode.h"
#include "arraylist.h"
#include "sealed.h"

namespace WASM {

	class Function {
	public:
		Function(ModuleFunctionIndex idx)
			: mModuleIndex{ idx } {}

		virtual ~Function() = default;

		ModuleFunctionIndex moduleIndex() const { return mModuleIndex; }
		Nullable<const std::string> lookupName(const Module&) const;

		virtual Nullable<const BytecodeFunction> asBytecodeFunction() const { return {}; }
		virtual Nullable<const HostFunctionBase> asHostFunction() const { return {}; }
		virtual const FunctionType& functionType() const = 0;
		InterpreterTypeIndex interpreterTypeIndex() const { return mInterpreterTypeIndex; }

		// Const-cast helpers
		auto asBytecodeFunction() { return Nullable<BytecodeFunction>::fromPointer( const_cast<BytecodeFunction*>(static_cast<const Function*>(this)->asBytecodeFunction().pointer()) ); }
		auto asHostFunction() { return Nullable<HostFunctionBase>::fromPointer(const_cast<HostFunctionBase*>(static_cast<const Function*>(this)->asHostFunction().pointer())); }

	protected:
		ModuleFunctionIndex mModuleIndex;
		InterpreterTypeIndex mInterpreterTypeIndex{ 0 };
	};

	class BytecodeFunction final : public Function {
	public:
		struct LocalOffset {
			ValType type;
			u32 offset;
		};

		// size of RA + FP + SP + MP
		static constexpr u32 SpecialFrameBytes = 32;

		BytecodeFunction(ModuleFunctionIndex idx, ModuleTypeIndex ti, const FunctionType& t, FunctionCode c);

		virtual Nullable<const BytecodeFunction> asBytecodeFunction() const { return *this; }

		ModuleTypeIndex moduleTypeIndex() const { return mModuleTypeIndex; }
		const Expression& expression() const { return code; }

		void setLinkedFunctionType(InterpreterTypeIndex idx, FunctionType& ft) { mInterpreterTypeIndex = idx;  type = ft; }
		virtual const FunctionType& functionType() const override { return *type; }

		u32 maxStackHeight() const { return mMaxStackHeight; }
		void setMaxStackHeight(u32 h) { mMaxStackHeight = h; }
		const Buffer& bytecode() const { return mBytecode; }
		void setBytecode(Buffer b) { mBytecode = std::move(b); }

		std::optional<LocalOffset> localOrParameterByIndex(u32) const;
		bool hasLocals() const;
		u32 localsCount() const;
		u32 operandStackSectionOffsetInBytes() const;
		u32 localsSizeInBytes() const;
		bool requiresMemoryInstance() const;

	private:
		void uncompressLocalTypes(const std::vector<CompressedLocalTypes>&);

		ModuleTypeIndex mModuleTypeIndex;
		NonNull<const FunctionType> type;
		Expression code;
		std::vector<LocalOffset> uncompressedLocals;
		u32 mMaxStackHeight{ 0 };
		Buffer mBytecode;
	};

	class FunctionTable {
	public:
		FunctionTable(ModuleTableIndex, const TableType&);

		auto& limits() const { return mLimits; }
		ValType type() const { return mType; }
		u32 size() const { return (u32)table.size(); }

		i32 grow(u32, Nullable<Function>);
		void init(const LinkedElement&, u32, u32, u32);
		void copy(const FunctionTable&, u32, u32, u32);
		void fill(Nullable<Function>, u32, u32);

		__forceinline Nullable<Function> at(u32 idx) {
			if (idx > table.size()) {
				throw std::runtime_error{ "Out of bounds table access" };
			}
			return table[idx];
		}

		__forceinline void set(u32 idx, Nullable<Function> func) {
			if (idx > table.size()) {
				throw std::runtime_error{ "Out of bounds table access" };
			}
			table[idx]= func;
		}

	private:
		ModuleTableIndex index;
		ValType mType;
		Limits mLimits;
		std::vector<Nullable<Function>> table;
	};

	class LinkedElement {
	public:
		LinkedElement(ModuleElementIndex idx, ElementMode m, ValType t, ModuleTableIndex i, u32 o, std::vector<Nullable<Function>> f)
			: index{ idx }, mMode{ m }, refType{ t }, tableIndex{ i }, tableOffset{ o }, mFunctions{ std::move(f) } {}

		LinkedElement(ModuleElementIndex idx, ElementMode m, ValType t, ModuleTableIndex i, u32 o)
			: index{ idx }, mMode{ m }, refType{ t }, tableIndex{ i }, tableOffset{ o } {
			assert(mMode == ElementMode::Passive);
		}

		sizeType initTableIfActive(std::span<FunctionTable>);
		void drop();

		ValType referenceType() const { return refType; }
		const std::vector<Nullable<Function>>& references() const { return mFunctions; }

	private:
		ModuleElementIndex index;
		ElementMode mMode;
		ValType refType;
		ModuleTableIndex tableIndex;
		u32 tableOffset;
		std::vector<Nullable<Function>> mFunctions;
	};

	class LinkedDataItem {
	public:
		LinkedDataItem(ModuleDataIndex i, DataItemMode m, BufferSlice d)
			: mModuleIndex{ i }, mMode{ m }, mDataBytes{ std::move(d) }, mMemoryIndex{0}, mMemoryOffset{0} {}

		LinkedDataItem(ModuleDataIndex i, DataItemMode m, BufferSlice d, InterpreterMemoryIndex x, u32 o)
			: mModuleIndex{ i }, mMode{ m }, mDataBytes{ std::move(d) }, mMemoryIndex{ x }, mMemoryOffset{ o } {}

		sizeType initMemoryIfActive(Module&) const;

		auto& dataBytes() const { return mDataBytes; }

	private:
		ModuleDataIndex mModuleIndex;
		DataItemMode mMode;
		BufferSlice mDataBytes;
		InterpreterMemoryIndex mMemoryIndex;
		u32 mMemoryOffset;
	};

	class Memory {
	public:
		static constexpr u64 PageSize = 65536;

		Memory(ModuleMemoryIndex, Limits l);

		i32 grow(i32);
		void init(const LinkedDataItem&, u32, u32, u32);

		auto& limits() const { return mLimits; }
		u64 minBytes() const;
		std::optional<u64> maxBytes() const;
		sizeType currentSizeInPages() const;
		sizeType currentSizeInBytes() const;

		__forceinline u8* pointer(u32 idx) {
			if (idx > mData.size()) {
				throw std::runtime_error{ "Out of bounds memory access" };
			}
			return mData.data()+ idx;
		}

	private:
		ModuleMemoryIndex mIndex;
		Limits mLimits;
		std::vector<u8> mData;
	};

	class GlobalBase {};

	template<typename T>
	class Global : public GlobalBase {
	public:
		Global() = default;

		template<typename U>
		U& get() const {
			static_assert(sizeof(U) <= siezof(T) && alignof(U) <= alignof(T));
			return *(U*)(&storage);
		}

		void set(T val) { storage = val; }

	private:
		T storage{ 0 };
	};

	struct ResolvedGlobal {
		const GlobalBase& instance;
		const GlobalType& type;
	};

	class ModuleBase {
	public:
		ModuleBase(Interpreter& i) : mInterpreter{ i } {}

		virtual Nullable<HostModule> asHostModule() { return {}; }
		virtual Nullable<Module> asWasmModule() { return {}; }
		virtual std::string_view name() const = 0;
		virtual Nullable<Function> exportedFunctionByName(const std::string&) = 0;
		virtual Nullable<FunctionTable> exportedTableByName(const std::string&) = 0;
		virtual Nullable<Memory> exportedMemoryByName(const std::string&) = 0;
		virtual std::optional<ResolvedGlobal> exportedGlobalByName(const std::string&) = 0;

		Nullable<const HostModule> asHostModule() const { return const_cast<ModuleBase&>(*this).asHostModule(); }
		Nullable<const Module> asWasmModule() const { return const_cast<ModuleBase&>(*this).asWasmModule(); }

	protected:
		virtual void instantiate(ModuleLinker&, Nullable<Introspector>) = 0;
		void createMemoryBase(const MemoryType&, ModuleLinker&, Nullable<Introspector>);
		void createGlobalsBase(VirtualForwardIterator<DeclaredGlobalBase>&, ModuleLinker&, Nullable<Introspector>);

		virtual void initializeInstance(ModuleLinker&, Nullable<Introspector>)= 0;

		std::optional<InterpreterMemoryIndex> mMemoryIndex;
		IndexSpan<InterpreterGlobalTypedArrayIndex, Global<u32>> mGlobals32;
		IndexSpan<InterpreterGlobalTypedArrayIndex, Global<u64>> mGlobals64;
		Nullable<Memory> mLinkedMemory;

		NonNull<Interpreter> mInterpreter;
	};

	using ExportTable = std::unordered_map<std::string, ExportItem>;

	class Module final : public ModuleBase {
	public:
		Module(
			Interpreter&,
			Buffer,
			std::string,
			std::string,
			std::unique_ptr<ParsingState>,
			ExportTable
		);
		Module(Module&& m) = default;

		virtual Nullable<Module> asWasmModule() override { return *this; }
		virtual std::string_view name() const override { return mName; }

		bool needsLinking() const { return compilationData != nullptr; }

		Nullable<Function> functionByIndex(ModuleFunctionIndex);
		std::optional<ResolvedGlobal> globalByIndex(ModuleGlobalIndex);
		Nullable<Memory> memoryByIndex(ModuleMemoryIndex);
		Nullable<FunctionTable> tableByIndex(ModuleTableIndex);
		Nullable<LinkedElement> linkedElementByIndex(ModuleElementIndex);
		Nullable<LinkedDataItem> linkedDataItemByIndex(ModuleDataIndex);
		Nullable<Function> startFunction() const { return mLinkedStartFunction; }
		Nullable<Memory> memoryWithIndexZero() const { return mLinkedMemory; }

		Nullable<const Function> findFunctionByBytecodePointer(const u8*) const;

		std::optional<ExportItem> exportByName(const std::string&, ExportType) const;
		Nullable<const std::string> functionNameByIndex(ModuleFunctionIndex) const;
		virtual Nullable<Function> exportedFunctionByName(const std::string&) override;
		virtual Nullable<FunctionTable> exportedTableByName(const std::string&) override;
		virtual Nullable<Memory> exportedMemoryByName(const std::string&) override;
		virtual std::optional<ResolvedGlobal> exportedGlobalByName(const std::string&) override;

	private:
		friend class ModuleCompiler;
		friend class ModuleLinker;

		virtual void instantiate(ModuleLinker&, Nullable<Introspector>) override;
		void createFunctions(ModuleLinker&, Nullable<Introspector>);
		void createMemory(ModuleLinker&, Nullable<Introspector>);
		void createTables(ModuleLinker&, Nullable<Introspector>);
		void createGlobals(ModuleLinker&, Nullable<Introspector>);
		
		virtual void initializeInstance(ModuleLinker&, Nullable<Introspector>) override;
		void createElementsAndInitTables(ModuleLinker&, Nullable<Introspector>);
		void createDataItemsAndInitMemory(ModuleLinker&, Nullable<Introspector>);

		std::string mPath;
		std::string mName;
		Buffer mData;

		IndexSpan<InterpreterFunctionIndex, BytecodeFunction> mFunctions;
		IndexSpan<InterpreterTableIndex, FunctionTable> mTables;
		IndexSpan<InterpreterLinkedElementIndex, LinkedElement> mElements;
		IndexSpan<InterpreterLinkedDataIndex, LinkedDataItem> mDataItems;
		Nullable<Function> mLinkedStartFunction;

		std::unique_ptr<ParsingState> compilationData;
		ExportTable exports;

		u32 numImportedFunctions;
		u32 numImportedTables;
		u32 numImportedMemories;
		u32 numImportedGlobals;

		ParsingState::NameMap functionNameMap;
	};

	class ModuleLinker {
	public:
		ModuleLinker(Interpreter& inter, Nullable<Introspector> intro)
			: interpreter{ inter }, introspector{ intro } {}

		void link();
		void storeLinkedItems();

		std::vector<BytecodeFunction>& createFunctions(u32);
		std::vector<FunctionTable>& createTables(u32);
		std::vector<LinkedElement>& createElements(u32);
		std::vector<Memory>& createMemory();
		std::vector<LinkedDataItem>& createDataItems(u32);
		std::vector<Global<u32>>& createGlobals32(u32);
		std::vector<Global<u64>>& createGlobals64(u32);

		sizeType currentNumGlobals32() const;
		sizeType currentNumGlobals64() const;

	private:
		struct DependencyItem {
			NonNull<Imported> import;
			NonNull<const Module> importingModule;
			NonNull<Module> exportingModule;
			ExportItem exportedItem{ ExportType::FunctionIndex };
		};

		void checkModulesLinkStatus();
		void instantiateModules();
		void buildDeduplicatedFunctionTypeTable();
		sizeType countDependencyItems();
		void createDependencyItems(const Module&, VirtualSpan<Imported>);
		void linkDependencies();
		void addDepenencyItem(DependencyItem);
		void initGlobals();
		void initializeHostModules();
		void initializeWasmModules();
		void linkMemoryInstances();
		void linkStartFunctions();

		void throwLinkError(const Module&, const Imported&, const char*) const;
		void throwLinkError(const Module&, const char*, const char*) const;

		std::vector<FunctionType> allFunctionTypes;
		std::vector<BytecodeFunction> allFunctions;
		std::vector<FunctionTable> allTables;
		std::vector<LinkedElement> allElements;
		std::vector<Memory> allMemories;
		std::vector<LinkedDataItem> allDataItems;
		std::vector<Global<u32>> allGlobals32;
		std::vector<Global<u64>> allGlobals64;

		ArrayList<DependencyItem> unresolvedImports;
		std::optional<sizeType> listBegin;

		Interpreter& interpreter;
		Nullable<Introspector> introspector;
	};

	class ModuleCompiler {
	public:
		ModuleCompiler(Interpreter& i, Module& m, Nullable<Introspector> in)
			: interpreter{ i }, module{ m }, introspector{ in } {}

		void compile();

		static void printBytecode(std::ostream&, const Buffer&);

	private:
		using ValueRecord = std::optional<ValType>;
		
		class LabelTypes {
		public:
			using Storage = std::variant<BlockTypeParameters, BlockTypeResults>;

			LabelTypes(Storage s) : storage{ std::move(s) } {}

			bool isParameters() const { return storage.index() == 0; }
			const BlockTypeParameters& asParameters() const { return std::get<0>(storage); }
			const BlockTypeResults& asResults() const { return std::get<1>(storage); }

			std::optional<sizeType> size(const Module&) const;

		private:
			Storage storage;
		};

		struct AddressPatchRequest {
			sizeType locationToPatch;
			sizeType jumpReferencePosition;
			bool isNearJump;
		};

		struct ControlFrame {
			InstructionType opCode;
			BlockTypeIndex blockTypeIndex;
			u32 height;
			u32 heightInBytes;
			bool unreachable{ false };
			u32 bytecodeOffset;
			std::optional<sizeType> addressPatchList;
			std::optional<AddressPatchRequest> elseLabelAddressPatch;

			LabelTypes labelTypes() const;
			void appendAddressPatchRequest(ModuleCompiler&, AddressPatchRequest);
			void processAddressPatchRequests(ModuleCompiler&);
		};

		void compileFunction(BytecodeFunction&);
		
		void resetBytecodePrinter();
		void print(Bytecode c);
		void printU8(u8 x);
		void printU32(u32 x);
		void printU64(u64 x);
		void printF32(f32 f);
		void printF64(f64 f);
		void printPointer(const void* p);

		void printBytecodeExpectingNoArgumentsIfReachable(Instruction);
		void printLocalGetSetTeeBytecodeIfReachable(BytecodeFunction::LocalOffset, Bytecode, Bytecode, Bytecode, Bytecode);

		// Based on the expression validation algorithm
		void setFunctionContext(const BytecodeFunction&);
		void pushValue(ValType);
		void pushMaybeValue(ValueRecord);
		ValueRecord popValue();
		ValueRecord popValue(ValueRecord);
		void pushValues(std::span<const ValType>);
		void pushValues(std::span<const ValueRecord>);
		void pushValues(const BlockTypeParameters&);
		void pushValues(const BlockTypeResults&);
		void pushValues(const LabelTypes&);
		void popValues(std::span<const ValueRecord>);
		void popValues(std::span<const ValType>);
		void popValues(const BlockTypeParameters&);
		void popValues(const BlockTypeResults&);
		void popValues(const LabelTypes&);
		const std::vector<ValueRecord>& popValuesToList(std::span<const ValType>);
		const std::vector<ValueRecord>& popValuesToList(const BlockTypeParameters&);
		const std::vector<ValueRecord>& popValuesToList(const BlockTypeResults&);
		const std::vector<ValueRecord>& popValuesToList(const LabelTypes& types);
		ControlFrame& pushControlFrame(InstructionType, BlockTypeIndex);
		ControlFrame popControlFrame();
		void setUnreachable();
		bool isReachable() const;
		void compileNumericConstantInstruction(Instruction);
		void compileNumericUnaryInstruction(Instruction);
		void compileNumericBinaryInstruction(Instruction);
		void compileMemoryDataInstruction(Instruction);
		void compileMemoryControlInstruction(Instruction);
		void compileBranchTableInstruction(Instruction);
		void compileTableInstruction(Instruction);
		void compileInstruction(Instruction, u32);
		void resetCachedReturnList(u32);

		BytecodeFunction::LocalOffset localByIndex(u32) const;
		ResolvedGlobal globalByIndex(ModuleGlobalIndex) const;
		const FunctionType& blockTypeByIndex(ModuleTypeIndex);
		const Memory& memoryByIndex(ModuleMemoryIndex);
		const LinkedElement& linkedElementByIndex(ModuleElementIndex) const;
		const LinkedDataItem& linkedDataItemByIndex(ModuleDataIndex) const;
		u32 measureMaxPrintedBlockLength(u32, u32, bool= false) const;
		void requestAddressPatch(u32, bool, bool = false, std::optional<u32> jumpReferencePosition = {});
		void patchAddress(const AddressPatchRequest&);

		void printBytecode(std::ostream&);

		void throwCompilationError(const char*) const;

		Interpreter& interpreter;
		Module& module;
		Nullable<Introspector> introspector;

		Buffer printedBytecode;

		u32 stackHeightInBytes{ 0 };
		u32 maxStackHeightInBytes{ 0 };
		std::vector<ValueRecord> valueStack;
		std::vector<ControlFrame> controlStack;
		std::vector<ValueRecord> cachedReturnList;

		ArrayList<AddressPatchRequest> addressPatches;
		
		const BytecodeFunction* currentFunction{ nullptr };
	};

}
