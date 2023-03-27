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
		std::optional<u32> constantFuncRefAsIndex() const;
		u64 constantUntypedValue() const;

	private:
		BufferSlice mBytes;
		std::vector<Instruction> mInstructions;
	};

	class FunctionType {
	public:
		FunctionType(std::vector<ValType> p, std::vector<ValType> r)
			: mParameters{ std::move(p) }, mResults{ std::move(r) } {}

		FunctionType(FunctionType&&) = default;

		const std::vector<ValType>& parameters() const { return mParameters; }
		const std::vector<ValType>& results() const { return mResults; }

		bool returnsVoid() const;
		bool takesVoidReturnsVoid() const;
		u32 parameterStackSectionSizeInBytes() const;
		u32 resultStackSectionSizeInBytes() const;
		void print(std::ostream&) const;

	private:
		std::vector<ValType> mParameters;
		std::vector<ValType> mResults;
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

	class DeclaredGlobal {
	public:
		DeclaredGlobal(GlobalType t, Expression c)
			: mType{ t }, mInitExpression{ std::move(c) } {}

		const GlobalType& type() const { return mType; }
		const ValType valType() const { return mType.valType(); }
		const Expression& initExpression() const { return mInitExpression; }

		void setIndexInTypedStorageArray(u32 idx);
		std::optional<u32> indexInTypedStorageArray() const { return mIndexInTypedStorageArray; }

		void print(std::ostream& out) const;

	private:
		GlobalType mType;
		Expression mInitExpression;
		std::optional<u32> mIndexInTypedStorageArray;
	};

	struct ExportItem {
		ExportType mExportType;
		u32 mIndex;
	};

	class Export final : private ExportItem {
	public:
		Export(std::string n, ExportType e, u32 i)
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
			u32 tableIndex;
			Expression tableOffset;
		};

		Element( ElementMode m, ValType r, std::vector<u32> f )
			: mMode{ m }, refType{r}, mInitExpressions{ std::move(f) } {}

		Element(ElementMode m, ValType r, u32 ti, Expression to, std::vector<u32> f)
			: mMode{ m }, refType{ r }, mTablePosition{ {ti, std::move(to)} }, mInitExpressions{ std::move(f) } {}

		Element(ElementMode m, ValType r, std::vector<Expression> e)
			: mMode{ m }, refType{ r }, mInitExpressions{ std::move(e) } {}

		Element(ElementMode m, ValType r, u32 ti, Expression to, std::vector<Expression> e)
			: mMode{ m }, refType{ r }, mTablePosition{ {ti, std::move(to)} }, mInitExpressions{ std::move(e) } {}

		u32 tableIndex() const {
			return mTablePosition.has_value() ? mTablePosition->tableIndex : 0;
		}

		ElementMode mode() const { return mMode; }
		ValType valType() const { return refType; }
		const std::optional<TablePosition>& tablePosition() const { return mTablePosition; }
		Nullable<const std::vector<Expression>> initExpressions() const;

		void print(std::ostream& out) const;
		DecodedElement decode(u32);

	private:
		ElementMode mMode;
		ValType refType;
		std::optional<TablePosition> mTablePosition;
		std::variant<std::vector<u32>, std::vector<Expression>> mInitExpressions;
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

	struct Imported {
		std::string module;
		std::string name;
	};

	struct FunctionImport final : public Imported {
		u32 functionTypeIndex;
		Nullable<Function> resolvedFunction;
	};

	struct TableImport final : public Imported {
		TableType tableType;
		Nullable<FunctionTable> resolvedTable;
	};

	struct MemoryImport final : public Imported {
		MemoryType memoryType;
		Nullable<Memory> resolvedMemory;
	};

	struct GlobalImport final : public Imported {
		GlobalImport(std::string m, std::string n, GlobalType tp)
			: Imported{ std::move(m), std::move(n) }, globalType{ tp }, resolvedGlobal32{} {}

		GlobalType globalType;

		Nullable<const GlobalBase> getBase() const;

	private:
		union {
			Nullable<Global<u32>> resolvedGlobal32;
			Nullable<Global<u64>> resolvedGlobal64;
		};
	};

	class ParsingState {
	public:
		using NameMap = std::unordered_map<u32, std::string>;
		using IndirectNameMap = std::unordered_map<u32, NameMap>;

	protected:
		friend class ModuleValidator;

		void clear();

		std::string path;
		Buffer data;
		BufferIterator it;
		std::unordered_map<std::string, BufferSlice> customSections;
		std::vector<FunctionType> functionTypes;
		std::vector<u32> functions;
		std::vector<TableType> tableTypes;
		std::vector<MemoryType> memoryTypes;
		std::vector<DeclaredGlobal> globals;
		std::vector<Export> exports;
		std::optional<u32> startFunction;
		std::vector<Element> elements;
		std::vector<FunctionCode> functionCodes;

		std::vector<FunctionImport> importedFunctions;
		std::vector<TableImport> importedTableTypes;
		std::vector<MemoryImport> importedMemoryTypes;
		std::vector<GlobalImport> importedGlobalTypes;

		std::string mName;
		NameMap functionNames;
		IndirectNameMap functionLocalNames;
	};

	class ModuleParser : public ParsingState {
	public:
		ModuleParser(Nullable<Introspector> intro) : introspector{ intro } {}

		void parse(Buffer, std::string);
		Module toModule();

	private:
		bool hasNext(u32 num = 1) const { return it.hasNext(num); }
		u8 nextU8() { return it.nextU8(); }
		void assertU8(u8 byte) { it.assertU8(byte); }

		u32 nextU32() { return it.nextU32(); }
		i32 nextI32() { return it.nextI32(); }

		u32 nextBigEndianU32() { return it.nextBigEndianU32(); }
		
		BufferSlice nextSliceOf(u32 length) { return it.nextSliceOf(length); }
		BufferSlice nextSliceTo(const BufferIterator& pos) { return it.nextSliceTo(pos); }
		BufferSlice sliceFrom(const BufferIterator& pos) const { return it.sliceFrom(pos); }

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

		NameMap parseNameMap();
		IndirectNameMap parseIndirectNameMap();

		FunctionType parseFunctionType();
		std::vector<ValType> parseResultTypeVector();

		TableType parseTableType();
		MemoryType parseMemoryType();
		GlobalType parseGlobalType();
		DeclaredGlobal parseGlobal();
		Limits parseLimits();
		Expression parseInitExpression();
		Export parseExport();
		Element parseElement();
		FunctionCode parseFunctionCode();

		std::vector<Expression> parseInitExpressionVector();
		std::vector<u32> parseU32Vector();

		void throwParsingError(const char*) const;

		Nullable<Introspector> introspector;
	};


	class ModuleValidator {
	public:
		ModuleValidator(Nullable<Introspector> intro) : introspector{ intro } {}

		void validate(const ParsingState&);

	private:
		const ParsingState& s() const { assert(parsingState); return *parsingState; }

		const FunctionType& functionTypeByIndex(u32);
		const TableType& tableTypeByIndex(u32);
		const MemoryType& memoryTypeByIndex(u32);
		const GlobalType& globalTypeByIndex(u32);

		void validateFunction(u32);
		void validateTableType(const TableType&);
		void validateMemoryType(const MemoryType&);
		void validateExport(const Export&);
		void validateStartFunction(u32);
		void validateGlobal(const DeclaredGlobal&);
		void validateElementSegment(const Element&);
		void validateImports();

		void validateConstantExpression(const Expression&, ValType);

		void throwValidationError(const char*) const;

		const ParsingState* parsingState{ nullptr };
		std::unordered_set<std::string> exportNames;

		Nullable<Introspector> introspector;
	};
}
