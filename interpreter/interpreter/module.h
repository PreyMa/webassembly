
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
		Function(u32 idx) : mIndex{ idx } {}
		virtual ~Function() = default;

		u32 index() const { return mIndex; }
		Nullable<const std::string> lookupName(const Module&) const;

		virtual Nullable<const BytecodeFunction> asBytecodeFunction() const { return {}; }
		virtual Nullable<const HostFunctionBase> asHostFunction() const { return {}; }
		virtual const FunctionType& functionType() const = 0;

	protected:
		u32 mIndex;
	};

	class BytecodeFunction final : public Function {
	public:
		struct LocalOffset {
			ValType type;
			u32 offset;
		};

		// size of RA + FP + SP + MP
		static constexpr u32 SpecialFrameBytes = 32;

		BytecodeFunction(u32 idx, u32 ti, FunctionType& t, FunctionCode&& c);

		virtual Nullable<const BytecodeFunction> asBytecodeFunction() const { return *this; }

		u32 moduleBaseTypeIndex() const { return mModuleBasedTypeIndex; }
		u32 deduplicatedTypeIndex() const { return mDeduplicatedTypeIndex; }
		const Expression& expression() const { return code; }

		void setLinkedFunctionType(u32 idx, FunctionType& ft) { mDeduplicatedTypeIndex= idx;  type = ft; }
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
		bool requiresModuleInstance() const;

	private:
		void uncompressLocalTypes(const std::vector<CompressedLocalTypes>&);

		u32 mModuleBasedTypeIndex;
		u32 mDeduplicatedTypeIndex{ 0 };
		NonNull<FunctionType> type;
		Expression code;
		std::vector<LocalOffset> uncompressedLocals;
		u32 mMaxStackHeight{ 0 };
		Buffer mBytecode;
	};

	class FunctionTable {
	public:
		FunctionTable(u32, const TableType&);

		ValType type() const { return mType; }

		i32 grow(i32, Nullable<Function>);
		void init(const LinkedElement&, u32, u32, u32);

	private:
		u32 index;
		ValType mType;
		Limits limits;
		std::vector<Nullable<Function>> table;
	};

	class LinkedElement {
	public:
		LinkedElement(u32 idx, ElementMode m, ValType t, u32 i, u32 o, std::vector<Nullable<Function>> f)
			: index{ idx }, mMode{ m }, refType{ t }, tableIndex{ i }, tableOffset{ o }, mFunctions{ std::move(f) } {}

		LinkedElement(u32 idx, ElementMode m, ValType t, u32 i, u32 o)
			: index{ idx }, mMode{ m }, refType{ t }, tableIndex{ i }, tableOffset{ o } {
			assert(mMode == ElementMode::Passive);
		}

		sizeType initTableIfActive(std::span<FunctionTable>);

		ValType referenceType() const { return refType; }
		const std::vector<Nullable<Function>>& references() const { return mFunctions; }

	private:
		u32 index;
		ElementMode mMode;
		ValType refType;
		u32 tableIndex;
		u32 tableOffset;
		std::vector<Nullable<Function>> mFunctions;
	};

	class Memory {
	public:
		static constexpr u64 PageSize = 65536;

		Memory(u32, Limits l);

		i32 grow(i32);

		u64 minBytes() const;
		std::optional<u64> maxBytes() const;
		sizeType currentSize() const;

		__forceinline u8* pointer(u32 idx) {
			if (idx > data.size()) {
				throw std::runtime_error{ "Out of bounds memory access" };
			}
			return data.data()+ idx;
		}

	private:
		u32 index;
		Limits limits;
		std::vector<u8> data;
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

	using ExportTable = std::unordered_map<std::string, ExportItem>;

	class Module {
	public:
		struct ResolvedGlobal {
			const GlobalBase& instance;
			const GlobalType& type;
		};

		Module(
			Buffer b,
			std::string p,
			std::string n,
			std::vector<FunctionType> ft,
			std::vector<BytecodeFunction> fs,
			std::vector<FunctionTable> ts,
			std::optional<Memory> ms,
			ExportTable ex,
			std::vector<DeclaredGlobal> gt,
			std::vector<Global<u32>> g64,
			std::vector<Global<u64>> g32,
			std::vector<Element> el,
			std::optional<u32> sf,
			std::vector<FunctionImport> imFs,
			std::vector<TableImport> imTs,
			std::optional<MemoryImport> imMs,
			std::vector<GlobalImport> imGs,
			ParsingState::NameMap fns
		);
		Module(Module&& m) = default;

		const std::string& name() const { return mName; }
		bool needsLinking() const { return compilationData != nullptr; }
		void initTables(Nullable<Introspector>);
		void initGlobals(Nullable<Introspector>);

		Nullable<Function> functionByIndex(u32);
		std::optional<ResolvedGlobal> globalByIndex(u32);
		Nullable<Memory> memoryByIndex(u32);
		Nullable<FunctionTable> tableByIndex(u32);
		Nullable<BytecodeFunction> startFunction() const { return mStartFunction; }
		Nullable<Memory> memoryWithIndexZero() const { return linkedMemory; }

		Nullable<const Function> findFunctionByBytecodePointer(const u8*) const;

		std::optional<ExportItem> exportByName(const std::string&, ExportType) const;
		Nullable<Function> exportedFunctionByName(const std::string&);
		Nullable<const std::string> functionNameByIndex(u32) const;

	private:
		friend class ModuleCompiler;
		friend class ModuleLinker;

		struct CompilationData {
			std::vector<FunctionImport> importedFunctions;
			std::vector<TableImport> importedTables;
			std::optional<MemoryImport> importedMemory;
			std::vector<GlobalImport> importedGlobals;
			std::vector<DeclaredGlobal> globalTypes;
			std::vector<Element> elements;
			std::optional<u32> startFunctionIndex;
			std::vector<FunctionType> functionTypes;
		};

		std::string path;
		std::string mName;
		Buffer data;

		SealedVector<BytecodeFunction> functions;
		SealedVector<FunctionTable> functionTables;
		std::optional<Memory> ownedMemoryInstance;
		SealedVector<Global<u32>> globals32;
		SealedVector<Global<u64>> globals64;
		std::vector<LinkedElement> elements;

		u32 numRemainingElements{ 0 };

		// Linked references
		Nullable<BytecodeFunction> mStartFunction;
		Nullable<Memory> linkedMemory;

		std::unique_ptr<CompilationData> compilationData;
		ExportTable exports;

		u32 numImportedFunctions;
		u32 numImportedTables;
		u32 numImportedMemories;
		u32 numImportedGlobals;

		ParsingState::NameMap functionNameMap;
	};

	class ModuleLinker {
	public:
		ModuleLinker(Interpreter& inter, std::span<Module> ms) : interpreter{ inter }, modules { ms } {}

		void link();

	private:
		void buildDeduplicatedFunctionTypeTable();

		Interpreter& interpreter;
		std::span<Module> modules;
	};

	class ModuleCompiler {
	public:
		ModuleCompiler(const Interpreter& i, Module& m)
			: interpreter{ i }, module{ m } {}

		void compile();

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
		Module::ResolvedGlobal globalByIndex(u32) const;
		const FunctionType& blockTypeByIndex(u32);
		const Memory& memoryByIndex(u32);
		u32 measureMaxPrintedBlockLength(u32, u32, bool= false) const;
		void requestAddressPatch(u32, bool, bool = false, std::optional<u32> jumpReferencePosition = {});
		void patchAddress(const AddressPatchRequest&);

		void printBytecode(std::ostream&);

		void throwCompilationError(const char*) const;

		const Interpreter& interpreter;
		Module& module;

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
