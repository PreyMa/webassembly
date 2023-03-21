
#pragma once

#include "decoding.h"
#include "bytecode.h"
#include "arraylist.h"

namespace WASM {

	class Function {
	public:
		virtual ~Function() = default;

		virtual Nullable<const BytecodeFunction> asBytecodeFunction() const { return {}; }

	private:
	};

	class BytecodeFunction : public Function {
	public:
		struct LocalOffset {
			ValType type;
			u32 offset;
		};

		BytecodeFunction(u32 idx, u32 ti, FunctionType& t, FunctionCode&& c);

		virtual Nullable<const BytecodeFunction> asBytecodeFunction() const { return *this; }

		u32 index() const { return mIndex; }
		u32 typeIndex() const { return mTypeIndex; }
		const Expression& expression() const { return code; }
		const FunctionType& functionType() const { return type; }

		std::optional<LocalOffset> localByIndex(u32) const;
		bool hasLocals() const;
		u32 operandStackSectionOffsetInBytes() const;
		u32 localsSizeInBytes() const;
		bool requiresModuleInstance() const;

	private:
		void uncompressLocalTypes(const std::vector<CompressedLocalTypes>&);

		u32 mIndex;
		u32 mTypeIndex;
		FunctionType& type;
		Expression code;
		std::vector<LocalOffset> uncompressedLocals;
	};

	class HostFunction : public Function {

	};

	class FunctionTable {
	public:
		FunctionTable(u32, const TableType&);

		i32 grow(i32, Nullable<Function>);

		void init(const DecodedElement&, u32, u32);

	private:
		u32 index;
		ValType type;
		Limits limits;
		std::vector<Nullable<Function>> table;
	};

	class DecodedElement {
	public:
		DecodedElement(u32 idx, ElementMode m, ValType t, u32 i, u32 o, std::vector<u32> f)
			: index{ idx }, mMode{ m }, refType{ t }, tableIndex{ i }, tableOffset{ o }, mFunctionIndices{ std::move(f) } {}

		void initTableIfActive(std::vector<FunctionTable>&);

	private:
		u32 index;
		ElementMode mMode;
		ValType refType;
		u32 tableIndex;
		u32 tableOffset;
		std::vector<u32> mFunctionIndices;
	};

	class Memory {
	public:
		static constexpr u64 PageSize = 65536;

		Memory(u32, Limits l);

		i32 grow(i32);

		u64 minBytes() const;
		std::optional<u64> maxBytes() const;

	private:
		u32 index;
		Limits limits;
		std::vector<u8> data;
	};

	template<typename T>
	class Global {
	public:
		Global() = default;

		template<typename U>
		U& get() const {
			static_assert(sizeof(U) <= siezof(T) && alignof(U) <= alignof(T));
			return *(U*)(&storage);
		}

	private:
		T storage{ 0 };
	};

	using ExportTable = std::unordered_map<std::string, ExportItem>;

	class Module {
	public:
		Module(
			Buffer b,
			std::string p,
			std::string n,
			std::vector<FunctionType> ft,
			std::vector<BytecodeFunction> fs,
			std::vector<FunctionTable> ts,
			std::vector<Memory> ms,
			ExportTable ex,
			std::vector<FunctionImport> imFs,
			std::vector<TableImport> imTs,
			std::vector<MemoryImport> imMs,
			std::vector<GlobalImport> imGs
		);
		Module(Module&& m) = default;

		const std::string& name() const { return mName; }
		bool needsLinking() const { return unlinkedImports.has_value(); }

		Nullable<Function> functionByIndex(u32);

		std::optional<ExportItem> exportByName(const std::string&, ExportType);
		Nullable<Function> exportedFunctionByName(const std::string&);

	private:
		friend class ModuleCompiler;

		struct UnlinkedImports {
			std::vector<FunctionImport> importedFunctions;
			std::vector<TableImport> importedTables;
			std::vector<MemoryImport> importedMemories;
			std::vector<GlobalImport> importedGlobals;
		};

		std::string path;
		std::string mName;
		Buffer data;

		std::vector<FunctionType> functionTypes;
		std::vector<BytecodeFunction> functions;
		std::vector<FunctionTable> functionTables;
		std::vector<Memory> memories;
		std::vector<Global<u32>> globals32;
		std::vector<Global<u64>> globals64;

		std::optional<UnlinkedImports> unlinkedImports;
		ExportTable exports;

		u32 numImportedFunctions;
		u32 numImportedTables;
		u32 numImportedMemories;
		u32 numImportedGlobals;
	};

	class ModuleLinker {
	public:
		void link();

	private:

	};

	class ModuleCompiler {
	public:
		ModuleCompiler(const Interpreter& i, Module& m)
			: interpreter{ i }, module{ m } {}

		void compile();

	private:
		using ValueRecord = std::optional<ValType>;
		using LabelTypes = std::variant<BlockTypeParameters, BlockTypeResults>;

		struct AddressPatchRequest {
			sizeType locationToPatch;
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
		ValueRecord popValue();
		ValueRecord popValue(ValueRecord);
		void pushValues(const std::vector<ValType>&);
		void pushValues(const BlockTypeParameters&);
		void pushValues(const BlockTypeResults&);
		void pushValues(const LabelTypes&);
		void popValues(const std::vector<ValueRecord>&);
		void popValues(const std::vector<ValType>&);
		void popValues(const BlockTypeParameters&);
		void popValues(const BlockTypeResults&);
		void popValues(const LabelTypes&);
		const std::vector<ValueRecord>& popValuesToList(const std::vector<ValType>&);
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
		void compileInstruction(Instruction, u32);
		void resetCachedReturnList(u32);

		BytecodeFunction::LocalOffset localByIndex(u32) const;
		const FunctionType& blockTypeByIndex(u32);
		u32 measureMaxPrintedBlockLength(u32, u32, bool= false) const;
		void requestAddressPatch(u32, bool, bool= false);
		void patchAddress(const AddressPatchRequest&);

		void printBytecode(std::ostream&);

		void throwCompilationError(const char*) const;

		const Interpreter& interpreter;
		Module& module;

		Buffer printedBytecode;

		u32 stackHeightInBytes;
		std::vector<ValueRecord> valueStack;
		std::vector<ControlFrame> controlStack;
		std::vector<ValueRecord> cachedReturnList;

		ArrayList<AddressPatchRequest> addressPatches;
		
		const BytecodeFunction* currentFunction{ nullptr };
	};

}
