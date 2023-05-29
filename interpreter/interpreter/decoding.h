#pragma once

#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>

#include "nullable.h"
#include "instruction.h"

namespace WASM {
	struct CompressedLocalTypes {
		u32 count;
		ValType type;
	};
	
	class Expression {
	public:
		Expression(BufferSlice b, std::vector<Instruction> i)
			: mBytes{ b }, mInstructions{ std::move(i) } {}

		const BufferSlice& bytes() const { return mBytes; }

		void printBytes(std::ostream&) const;
		void print(std::ostream&) const;

		auto size() const { return mInstructions.size(); }
		auto begin() const { return mInstructions.cbegin(); }
		auto end() const { return mInstructions.cend(); }
		const auto& operator[](sizeType idx) const { return mInstructions[idx]; }

		i32 constantI32() const;
		std::optional<ModuleFunctionIndex> constantFuncRefAsIndex() const;
		u64 constantUntypedValue(Module&) const;

	private:
		BufferSlice mBytes;
		std::vector<Instruction> mInstructions;
	};

	class FunctionType {
	public:
		FunctionType();
		FunctionType(std::span<const ValType>, std::span<const ValType>);
		FunctionType(const FunctionType&);
		FunctionType(FunctionType&&) = default;

		const std::span<const ValType> parameters() const;
		const std::span<const ValType> results() const;

		bool returnsVoid() const;
		bool takesVoidReturnsVoid() const;
		u32 parameterStackSectionSizeInBytes() const;
		u32 resultStackSectionSizeInBytes() const;
		bool takesValuesAsParameters(std::span<Value>) const;
		void print(std::ostream&) const;

		bool operator==(const FunctionType&) const;

	private:
		struct HeapArray {
			std::unique_ptr<ValType[]> array;
			sizeType numParameters;
			sizeType numResults;
		};

		struct LocalArray {
			static constexpr sizeType maxStoredEntries = (sizeof(HeapArray) - 2) / sizeof(ValType);
			ValType array[maxStoredEntries];
			u8 numParameters;
			u8 numResults;
		};

		bool isLocalArray() const { return storage.index() == 0; }
		const LocalArray& asLocalArray() const { assert(isLocalArray());  return std::get<0>(storage); }
		const HeapArray& asHeapArray() const { assert(!isLocalArray());  return std::get<1>(storage); }


		std::variant<LocalArray, HeapArray> storage;

		mutable std::optional<u32> requiredParameterStackBytes;
		mutable std::optional<u32> requiredResultStackBytes;
	};

	class Limits {
	public:
		Limits( u32 i ) : mMin{ i }, mMax{} {}
		Limits( u32 i, u32 a ) : mMin{ i }, mMax{ a } {}

		auto min() const { return mMin; }
		auto& max() const { return mMax; }

		void print(std::ostream&) const;
		bool isValid(u32 range) const;
		bool matches(const Limits&) const;
	
	private:
		u32 mMin;
		std::optional<u32> mMax;
	};

	class TableType {
	public:
		TableType(ValType et, Limits l) : mElementReferenceType{ et }, mLimits{ l } {
			assert( mElementReferenceType.isReference() );
		}

		const ValType valType() const { return mElementReferenceType; }
		const Limits& limits() const { return mLimits; }

		void print(std::ostream&) const;

	private:
		ValType mElementReferenceType;
		Limits mLimits;
	};

	class MemoryType {
	public:
		MemoryType(Limits l) : mLimits{ l } {}

		const Limits& limits() const { return mLimits; }

		void print(std::ostream& out) const { mLimits.print( out ); }

	private:
		Limits mLimits;
	};

	class GlobalType {
	public:
		GlobalType(ValType t, bool m)
			: mType{ t }, mIsMutable{ m } {}

		bool isMutable() const { return mIsMutable; }
		const ValType valType() const { return mType; }

	private:
		ValType mType;
		bool mIsMutable;
	};

	class DeclaredHostGlobal {
	public:
		DeclaredHostGlobal(GlobalType t)
			: mType{ t } {}

		const GlobalType& type() const { return mType; }
		const ValType valType() const { return mType.valType(); }

		void setIndexInTypedStorageArray(InterpreterGlobalTypedArrayIndex idx);
		auto indexInTypedStorageArray() const { return mIndexInTypedStorageArray; }

	protected:
		GlobalType mType;
		std::optional<InterpreterGlobalTypedArrayIndex> mIndexInTypedStorageArray;
	};

	class DeclaredGlobal final : public DeclaredHostGlobal {
	public:
		DeclaredGlobal(GlobalType t, Expression c)
			: DeclaredHostGlobal{ t }, mInitExpression{ std::move(c) } {}

		const Expression& initExpression() const { return mInitExpression; }
		void print(std::ostream& out) const;

	private:
		Expression mInitExpression;

	};

	struct ExportItem {
		ExportType mExportType;
		ModuleExportIndex mIndex;

		ModuleFunctionIndex asFunctionIndex() const;
		ModuleGlobalIndex asGlobalIndex() const;
		ModuleMemoryIndex asMemoryIndex() const;
		ModuleTableIndex asTableIndex() const;
	};

	class Export final : private ExportItem {
	public:
		Export(std::string n, ExportType e, ModuleExportIndex i)
			: ExportItem{ e, i }, mName { std::move(n) } {}

		const std::string& name() const { return mName; }
		std::string moveName() { return std::move(mName); }

		bool isValid(u32,u32,u32, u32) const;
		void print(std::ostream& out) const;

		ExportItem toItem() const { return *this; }

	private:
		std::string mName;
	};

	class Element {
	public:
		struct TablePosition {
			ModuleTableIndex tableIndex;
			Expression tableOffset;
		};

		Element( ElementMode m, ValType r, std::vector<ModuleFunctionIndex> f )
			: mMode{ m }, refType{r}, mInitExpressions{ std::move(f) } {}

		Element(ElementMode m, ValType r, ModuleTableIndex ti, Expression to, std::vector<ModuleFunctionIndex> f)
			: mMode{ m }, refType{ r }, mTablePosition{ {ti, std::move(to)} }, mInitExpressions{ std::move(f) } {}

		Element(ElementMode m, ValType r, std::vector<Expression> e)
			: mMode{ m }, refType{ r }, mInitExpressions{ std::move(e) } {}

		Element(ElementMode m, ValType r, ModuleTableIndex ti, Expression to, std::vector<Expression> e)
			: mMode{ m }, refType{ r }, mTablePosition{ {ti, std::move(to)} }, mInitExpressions{ std::move(e) } {}

		ModuleTableIndex tableIndex() const {
			return mTablePosition.has_value() ? mTablePosition->tableIndex : ModuleTableIndex{ 0 };
		}

		ElementMode mode() const { return mMode; }
		ValType valType() const { return refType; }
		const std::optional<TablePosition>& tablePosition() const { return mTablePosition; }
		Nullable<const std::vector<Expression>> initExpressions() const;

		void print(std::ostream& out) const;
		LinkedElement decodeAndLink(ModuleElementIndex, Module&);

	private:
		ElementMode mMode;
		ValType refType;
		std::optional<TablePosition> mTablePosition;
		std::variant<std::vector<ModuleFunctionIndex>, std::vector<Expression>> mInitExpressions;
	};

	class DataItem {
	public:
		struct MemoryPosition {
			ModuleMemoryIndex mMemoryIndex;
			Expression mOffsetExpression;
		};

		DataItem(DataItemMode m, BufferSlice b, ModuleMemoryIndex i, Expression e)
			: mMode{ m }, mDataBytes{ std::move(b) }, mMemoryPosition{ MemoryPosition{ i, std::move(e) } } {}

		DataItem(DataItemMode m, BufferSlice b)
			: mMode{ m }, mDataBytes{ std::move(b) } {}

		auto mode() const { return mMode; }
		const auto& memoryPosition() const { return mMemoryPosition; }

		void print(std::ostream&, bool showData= true) const;

	private:
		DataItemMode mMode;
		BufferSlice mDataBytes;
		std::optional<MemoryPosition> mMemoryPosition;
	};

	class FunctionCode {
	public:
		FunctionCode(Expression c, std::vector<CompressedLocalTypes> l)
			: code{ std::move(c) }, compressedLocalTypes{ std::move(l) } {}

		const auto& locals() const { return compressedLocalTypes; }

		auto begin() const { return code.begin(); }
		auto end() const { return code.end(); }

		void print(std::ostream& out) const;
		void printBody(std::ostream& out) const;

	private:
		friend class BytecodeFunction;

		Expression code;
		std::vector<CompressedLocalTypes> compressedLocalTypes;
	};

	class Imported {
	public:
		Imported(std::string m, std::string n) : mModule{ std::move(m) }, mName{ std::move(n) } {}

		auto& module() const { return mModule; }
		auto& name() const { return mName; }
		std::string scopedName() const;

		virtual ExportType requiredExportType() const = 0;
		virtual bool isResolved() const = 0;
		virtual bool tryResolveFromModuleWithIndex(Module&, ModuleExportIndex) = 0;
		virtual bool tryResolveFromModuleWithName(ModuleBase&) = 0;
		virtual bool isTypeCompatible() const = 0;

	protected:
		std::string mModule;
		std::string mName;
	};

	class FunctionImport final : public Imported {
	public:
		FunctionImport(std::string m, std::string n, ModuleTypeIndex idx)
			: Imported{ std::move(m), std::move(n) }, mModuleTypeIndex{ idx } {}

		auto& moduleTypeIndex() const { return mModuleTypeIndex; }
		auto& interpreterTypeIndex() const { return *mInterpreterTypeIndex; }
		auto& resolvedFunction() const { return mResolvedFunction; }

		bool hasInterpreterTypeIndex() const { return mInterpreterTypeIndex.has_value(); }
		void interpreterTypeIndex(InterpreterTypeIndex idx);

		virtual ExportType requiredExportType() const override;
		virtual bool isResolved() const override;
		virtual bool tryResolveFromModuleWithIndex(Module&, ModuleExportIndex) override;
		virtual bool tryResolveFromModuleWithName(ModuleBase&) override;
		virtual bool isTypeCompatible() const override;

	private:
		ModuleTypeIndex mModuleTypeIndex;
		std::optional<InterpreterTypeIndex> mInterpreterTypeIndex;
		Nullable<Function> mResolvedFunction;
	};

	class TableImport final : public Imported {
	public:
		TableImport(std::string m, std::string n, TableType type)
			: Imported{ std::move(m), std::move(n) }, mTableType{ type } {}

		auto& tableType() const { return mTableType; }
		auto& resolvedTable() const { return mResolvedTable; }

		virtual ExportType requiredExportType() const override;
		virtual bool isResolved() const override;
		virtual bool tryResolveFromModuleWithIndex(Module&, ModuleExportIndex) override;
		virtual bool tryResolveFromModuleWithName(ModuleBase&) override;
		virtual bool isTypeCompatible() const override;

	private:
		TableType mTableType;
		Nullable<FunctionTable> mResolvedTable;
	};

	class MemoryImport final : public Imported {
	public:
		MemoryImport(std::string m, std::string n, MemoryType type)
			: Imported{ std::move(m), std::move(n) }, mMemoryType{ type } {}

		auto& memoryType() const { return mMemoryType; }
		auto& resolvedMemory() const { return mResolvedMemory; }

		virtual ExportType requiredExportType() const override;
		virtual bool isResolved() const override;
		virtual bool tryResolveFromModuleWithIndex(Module&, ModuleExportIndex) override;
		virtual bool tryResolveFromModuleWithName(ModuleBase&) override;
		virtual bool isTypeCompatible() const override;

	private:
		MemoryType mMemoryType;
		Nullable<Memory> mResolvedMemory;
	};

	class GlobalImport final : public Imported {
	public:
		GlobalImport(std::string m, std::string n, GlobalType tp)
			: Imported{ std::move(m), std::move(n) }, mGlobalType{ tp }, mResolvedGlobal32{} {}

		auto& globalType() const { return mGlobalType; }

		Nullable<const GlobalBase> getBase() const;
		virtual ExportType requiredExportType() const override;
		virtual bool isResolved() const override;
		virtual bool tryResolveFromModuleWithIndex(Module&, ModuleExportIndex) override;
		virtual bool tryResolveFromModuleWithName(ModuleBase&) override;
		virtual bool isTypeCompatible() const override;

	private:
		bool resolveFromResolvedGlobal(std::optional<ResolvedGlobal>);

		GlobalType mGlobalType;

		union {
			Nullable<Global<u32>> mResolvedGlobal32;
			Nullable<Global<u64>> mResolvedGlobal64;
		};
	};

	class ParsingState {
	public:
		using NameMap = std::unordered_map<u32, std::string>;
		using IndirectNameMap = std::unordered_map<u32, NameMap>;

		auto& path() const { return mPath; }
		auto& customSections() const { return mCustomSections; }
		auto& functionTypes() const { return mFunctionTypes; }
		auto& functions() const { return mFunctions; }
		auto& tableTypes() const { return mTableTypes; }
		auto& memoryTypes() const { return mMemoryTypes; }
		auto& globals() const { return mGlobals; }
		auto& exports() const { return mExports; }
		auto& startFunctionIndex() const { return mStartFunctionIndex; }
		auto& elements() const { return mElements; }
		auto& functionCodes() const { return mFunctionCodes; }
		auto& importedFunctions() const { return mImportedFunctions; }
		auto& importedTableTypes() const { return mImportedTableTypes; }
		auto& importedMemoryTypes() const { return mImportedMemoryTypes; }
		auto& importedGlobalTypes() const { return mImportedGlobalTypes; }
		auto& expectedDataSectionCount() const { return mExpectedDataSectionCount; }
		auto& dataItems() const { return mDataItems; }
		auto& name() const { return mName; }
		auto& functionNames() const { return mFunctionNames; }
		auto& functionLocalNames() const { return mFunctionLocalNames; }

		// Getters that allow moving parts out of the parsing state into different storage
		auto releaseElements() const { return std::move(mElements); }
		auto releaseFunctionCodes() const { return std::move(mFunctionCodes); }
		auto releaseFunctionNames() const { return std::move(mFunctionNames); }
		auto releaseFunctionLocalNames() const { return std::move(mFunctionLocalNames); }

		auto& mutateImportedFunctions() { return mImportedFunctions; }
		auto& mutateImportedTableTypes() { return mImportedTableTypes; }
		auto& mutateImportedMemoryTypes() { return mImportedMemoryTypes; }
		auto& mutateImportedGlobalTypes() { return mImportedGlobalTypes; }

	protected:
		void clear();

		std::string mPath;
		Buffer mData;
		BufferIterator mIt;
		std::unordered_map<std::string, BufferSlice> mCustomSections;
		std::vector<FunctionType> mFunctionTypes;
		std::vector<ModuleTypeIndex> mFunctions;
		std::vector<TableType> mTableTypes;
		std::vector<MemoryType> mMemoryTypes;
		std::vector<DeclaredGlobal> mGlobals;
		std::vector<Export> mExports;
		std::optional<ModuleFunctionIndex> mStartFunctionIndex;
		std::vector<Element> mElements;
		std::vector<FunctionCode> mFunctionCodes;
		std::optional<u32> mExpectedDataSectionCount;
		std::vector<DataItem> mDataItems;

		std::vector<FunctionImport> mImportedFunctions;
		std::vector<TableImport> mImportedTableTypes;
		std::vector<MemoryImport> mImportedMemoryTypes;
		std::vector<GlobalImport> mImportedGlobalTypes;

		std::string mName;
		NameMap mFunctionNames;
		IndirectNameMap mFunctionLocalNames;
	};

	class ModuleParser : public ParsingState {
	public:
		ModuleParser(Nullable<Introspector> intro) : introspector{ intro } {}

		void parse(Buffer, std::string);
		Module toModule(Interpreter&);

	private:
		bool hasNext(u32 num = 1) const { return mIt.hasNext(num); }
		u8 nextU8() { return mIt.nextU8(); }
		void assertU8(u8 byte) { mIt.assertU8(byte); }

		u32 nextU32() { return mIt.nextU32(); }
		i32 nextI32() { return mIt.nextI32(); }

		u32 nextBigEndianU32() { return mIt.nextBigEndianU32(); }
		
		BufferSlice nextSliceOf(u32 length) { return mIt.nextSliceOf(length); }
		BufferSlice nextSliceTo(const BufferIterator& pos) { return mIt.nextSliceTo(pos); }
		BufferSlice sliceFrom(const BufferIterator& pos) const { return mIt.sliceFrom(pos); }

		std::string parseNameString();

		void parseHeader();
		void parseSection();
		void parseCustomSection(u32);
		void parseNameSection(BufferIterator);
		void parseTypeSection();
		void parseFunctionSection();
		void parseTableSection();
		void parseMemorySection();
		void parseGlobalSection();
		void parseExportSection();
		void parseStartSection();
		void parseElementSection();
		void parseCodeSection();
		void parseImportSection();
		void parseDataSection();
		void parseDataCountSection();

		NameMap parseNameMap();
		IndirectNameMap parseIndirectNameMap();

		FunctionType parseFunctionType();
		std::vector<ValType>& parseResultTypeVector(bool= true);

		TableType parseTableType();
		MemoryType parseMemoryType();
		GlobalType parseGlobalType();
		DeclaredGlobal parseGlobal();
		Limits parseLimits();
		Expression parseInitExpression();
		Export parseExport();
		Element parseElement();
		FunctionCode parseFunctionCode();
		DataItem parseDataItem();

		std::vector<Expression> parseInitExpressionVector();
		std::vector<ModuleFunctionIndex> parseU32Vector();
		BufferSlice parseU8Vector();

		void throwParsingError(const char*) const;

		Nullable<Introspector> introspector;
		std::vector<ValType> cachedResultTypeVector;
	};


	class ModuleValidator {
	public:
		ModuleValidator(Nullable<Introspector> intro) : introspector{ intro } {}

		void validate(const ParsingState&);

	private:
		const ParsingState& s() const { assert(parsingState); return *parsingState; }

		const FunctionType& functionTypeByIndex(ModuleFunctionIndex);
		const TableType& tableTypeByIndex(ModuleTableIndex);
		const MemoryType& memoryTypeByIndex(ModuleMemoryIndex);
		const GlobalType& globalTypeByIndex(ModuleGlobalIndex);

		void validateFunction(LocalFunctionIndex);
		void validateTableType(const TableType&);
		void validateMemoryType(const MemoryType&);
		void validateExport(const Export&);
		void validateStartFunction(ModuleFunctionIndex);
		void validateGlobal(const DeclaredGlobal&);
		void validateElementSegment(const Element&);
		void validateImports();
		void validateDataItem(const DataItem&);

		void validateConstantExpression(const Expression&, ValType);

		void throwValidationError(const char*) const;

		const ParsingState* parsingState{ nullptr };
		std::unordered_set<std::string> exportNames;

		Nullable<Introspector> introspector;
	};
}
