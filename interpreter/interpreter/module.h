#pragma once

#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <variant>

#include "nullable.h"
#include "instruction.h"

namespace WASM {

	class Function {
	private:
		std::vector<u8> code;
	};


	class Module {
	public:
		Module(Buffer b, std::string n) : name {std::move(n)}, data{ std::move(b) } {}
		Module(Module&& m) = default;

	private:
		std::string name;
		Buffer data;
	};

	class Expression {
	public:
		Expression(BufferSlice b, std::vector<Instruction> i)
			: mBytes{ b }, mInstructions{ std::move(i) } {}

		void printBytes(std::ostream&) const;
		void print(std::ostream&) const;

		auto size() const { return mInstructions.size(); }
		auto begin() const { return mInstructions.cbegin(); }
		auto end() const { return mInstructions.cend(); }

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

		bool takesVoidReturnsVoid() const;
		void print(std::ostream&) const;

	private:
		std::vector<ValType> mParameters;
		std::vector<ValType> mResults;
	};

	class Limits {
	public:
		Limits( u32 i ) : min{ i }, max{} {}
		Limits( u32 i, u32 a ) : min{ i }, max{ a } {}

		void print(std::ostream&) const;
		bool isValid(u32 range) const;
	
	private:
		u32 min;
		std::optional<u32> max;
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

	class Global {
	public:
		Global(ValType t, bool m, Expression c)
			: mType{ t }, mIsMutable{ m }, mInitExpression{ std::move(c) } {}

		const ValType valType() const { return mType; }
		const Expression& initExpression() const { return mInitExpression; }

		void print(std::ostream& out) const;

	private:
		ValType mType;
		bool mIsMutable;
		Expression mInitExpression;
	};

	class Export {
	public:
		Export(std::string n, ExportType e, u32 i)
			: mName{ std::move(n) }, mExportType{ e }, mIndex{ i } {}

		const std::string& name() const { return mName; }

		bool isValid(u32,u32,u32, u32) const;
		void print(std::ostream& out) const;

	private:
		std::string mName;
		ExportType mExportType;
		u32 mIndex;
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

	private:
		ElementMode mMode;
		ValType refType;
		std::optional<TablePosition> mTablePosition;
		std::variant<std::vector<u32>, std::vector<Expression>> mInitExpressions;
	};

	class FunctionCode {
	public:
		struct CompressedLocalTypes {
			u32 count;
			ValType type;
		};

		FunctionCode(Expression c, std::vector<CompressedLocalTypes> l)
			: code{ std::move(c) }, compressedLocalTypes{ std::move(l) } {}

		const std::vector<CompressedLocalTypes>& locals() const { return compressedLocalTypes; }

		auto begin() const { return code.begin(); }
		auto end() const { return code.end(); }

		void print(std::ostream& out) const;
		void printBody(std::ostream& out) const;

	private:
		Expression code;
		std::vector<CompressedLocalTypes> compressedLocalTypes;
	};

	class ParsingState {
	public:
		using NameMap = std::unordered_map<u32, std::string>;
		using IndirectNameMap = std::unordered_map<u32, NameMap>;

	protected:
		friend class ModuleValidator;

		std::string path;
		Buffer data;
		BufferIterator it;
		std::unordered_map<std::string, BufferSlice> customSections;
		std::vector<FunctionType> functionTypes;
		std::vector<u32> functions;
		std::vector<TableType> tableTypes;
		std::vector<MemoryType> memoryTypes;
		std::vector<Global> globals;
		std::vector<Export> exports;
		std::optional<u32> startFunction;
		std::vector<Element> elements;
		std::vector<FunctionCode> functionCodes;

		std::vector<u32> importedFunctions;
		std::vector<TableType> importedTableTypes;
		std::vector<MemoryType> importedMemoryTypes;
		std::vector<Global> importedGlobalTypes;

		std::string mName;
		NameMap functionNames;
		IndirectNameMap functionLocalNames;
	};


	class ModuleParser : public ParsingState {
	public:
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

		NameMap parseNameMap();
		IndirectNameMap parseIndirectNameMap();

		FunctionType parseFunctionType();
		std::vector<ValType> parseResultTypeVector();

		TableType parseTableType();
		MemoryType parseMemoryType();
		Global parseGlobal();
		Limits parseLimits();
		Expression parseInitExpression();
		Export parseExport();
		Element parseElement();
		FunctionCode parseFunctionCode();

		std::vector<Expression> parseInitExpressionVector();
		std::vector<u32> parseU32Vector();

		void throwParsingError(const char*) const;
	};

	class ModuleValidator {
	public:

		void validate(const ParsingState&);

	private:
		using ValueRecord = std::optional<ValType>;
		using LabelTypes = std::variant<BlockTypeParameters, BlockTypeResults>;

		struct ControlFrame {
			InstructionType opCode;
			BlockTypeIndex blockTypeIndex;
			u32 height;
			bool unreachable{ false };

			LabelTypes labelTypes() const;
		};


		const ParsingState& s() const { assert(parsingState); return *parsingState; }

		const FunctionType& functionTypeByIndex(u32);
		const TableType& tableTypeByIndex(u32);
		const MemoryType& memoryTypeByIndex(u32);
		const Global& globalTypeByIndex(u32);

		void validateFunction(u32);
		void validateTableType(const TableType&);
		void validateMemoryType(const MemoryType&);
		void validateExport(const Export&);
		void validateStartFunction(u32);
		void validateGlobal(const Global&);
		void validateElementSegment(const Element&);

		void validateConstantExpression(const Expression&, ValType);

		// Expression validation algorithm
		void setFunctionContext(const FunctionType&, const FunctionCode&);
		void pushValue(ValType);
		ValueRecord popValue();
		ValueRecord popValue(ValueRecord);
		void pushValues(const std::vector<ValType>&);
		void pushValues(const BlockTypeParameters&);
		void pushValues(const BlockTypeResults&);
		void pushValues(const LabelTypes&);
		std::vector<ValueRecord>& popValues(const std::vector<ValueRecord>&);
		std::vector<ValueRecord>& popValues(const std::vector<ValType>&);
		std::vector<ValueRecord>& popValues(const BlockTypeParameters&);
		std::vector<ValueRecord>& popValues(const BlockTypeResults&);
		std::vector<ValueRecord>& popValues(const LabelTypes&);
		void pushControlFrame(InstructionType, BlockTypeIndex);
		ControlFrame popControlFrame();
		void setUnreachable();
		void validateOpcode(Instruction);
		void resetCachedReturnList(u32);

		ValType localByIndex(u32) const;

		void throwValidationError(const char*) const;

		const ParsingState* parsingState{ nullptr };
		std::unordered_set<std::string> exportNames;

		std::vector<ValueRecord> valueStack;
		std::vector<ControlFrame> controlStack;
		std::vector<ValueRecord> cachedReturnList;

		const FunctionType* currentFunctionType{ nullptr };
		const std::vector<FunctionCode::CompressedLocalTypes>* currentLocals{ nullptr };
	};
}
