#pragma once

#include <unordered_map>
#include <optional>
#include <variant>

#include "buffer.h"
#include "enum.h"

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


	class SectionType : public Enum<SectionType> {
	public:
		enum TEnum {
			Custom = 0,
			Type = 1,
			Import = 2,
			Function = 3,
			Table = 4,
			Memory = 5,
			Global = 6,
			Export = 7,
			Start = 8,
			Element = 9,
			Code = 10,
			Data = 11,
			DataCount = 12,
			NumberOfItems = 13
		};

		using Enum<SectionType>::Enum;
		SectionType(TEnum e) : Enum<SectionType>{ e } {}

		const char* name() const;
	};

	class ValType : public Enum<ValType> {
	public:
		enum TEnum {
			I32 = 0x7F,
			I64 = 0x7E,
			F32 = 0x7D,
			F64 = 0x7C,
			V128 = 0x7B,
			FuncRef = 0x70,
			ExternRef = 0x6F,
			NumberOfItems = 0x80
		};

		using Enum<ValType>::Enum;
		ValType(TEnum e) : Enum<ValType>{ e } {}

		bool isNumber() const;
		bool isVector() const;
		bool isReference() const;
		bool isValid() const;
		const char* name() const;
	};

	class ExportType : public Enum<ExportType> {
	public:
		enum TEnum {
			FunctionIndex = 0x00,
			TableIndex = 0x01,
			MemoryIndex = 0x02,
			GlobalIndex = 0x03,
			NumberOfItems
		};

		using Enum<ExportType>::Enum;
		ExportType(TEnum e) : Enum<ExportType>{ e } {}

		const char* name() const;
	};

	class ElementMode : public Enum<ElementMode> {
	public:
		enum TEnum {
			Passive = 0,
			Active = 1,
			Declarative = 2,
			NumberOfItems
		};

		using Enum<ElementMode>::Enum;
		ElementMode(TEnum e) : Enum<ElementMode>{ e } {}

		const char* name() const;
	};

	class NameSubsectionType : public Enum<NameSubsectionType> {
	public:
		enum TEnum {
			// Based on the extended name section proposal: https://github.com/WebAssembly/extended-name-section/blob/main/proposals/extended-name-section/Overview.md
			ModuleName = 0,
			FunctionNames = 1,
			LocalNames = 2,
			LabelNames = 3,
			TypeNames = 4,
			TableNames = 5,
			MemoryNames = 6,
			GlobalNames = 7,
			ElementSegmentNames = 8,
			DataSegmentNames = 9,
			NumberOfItems
		};

		using Enum<NameSubsectionType>::Enum;
		NameSubsectionType(TEnum e) : Enum<NameSubsectionType>{ e } {}

		const char* name() const;
	};

	class FunctionType {
	public:
		FunctionType(std::vector<ValType> p, std::vector<ValType> r)
			: parameters{ std::move(p) }, results{ std::move(r) } {}

		FunctionType(FunctionType&&) = default;

		void print(std::ostream&);

	private:
		std::vector<ValType> parameters;
		std::vector<ValType> results;
	};

	class Limits {
	public:
		Limits( u32 i ) : min{ i }, max{} {}
		Limits( u32 i, u32 a ) : min{ i }, max{ a } {}

		void print(std::ostream&);
	
	private:
		u32 min;
		std::optional<u32> max;
	};

	class TableType {
	public:
		TableType(ValType et, Limits l) : elementReferenceType{ et }, limits{ l } {
			assert( elementReferenceType.isReference() );
		}
		
		void print(std::ostream&);

	private:
		ValType elementReferenceType;
		Limits limits;
	};

	class MemoryType {
	public:
		MemoryType(Limits l) : limits{ l } {}

		void print(std::ostream& out) { limits.print( out ); }

	private:
		Limits limits;
	};

	class Global {
	public:
		Global(ValType t, bool m, BufferSlice c)
			: type{ t }, isMutable{ m }, initExpression{ c } {}

		void print(std::ostream& out);

	private:
		ValType type;
		bool isMutable;
		BufferSlice initExpression;
	};

	class Export {
	public:
		Export(std::string n, ExportType e, u32 i)
			: name{ std::move(n) }, exportType{ e }, index{ i } {}

		void print(std::ostream& out);

	private:
		std::string name;
		ExportType exportType;
		u32 index;
	};

	class Element {
	public:
		Element( ElementMode m, ValType r, std::vector<u32> f )
			: mode{ m }, refType{r}, initExpressions { std::move(f) } {}

		Element(ElementMode m, ValType r, u32 ti, BufferSlice to, std::vector<u32> f)
			: mode{ m }, refType{ r }, tablePosition{ {ti, to} }, initExpressions{ std::move(f) } {}

		Element(ElementMode m, ValType r, std::vector<BufferSlice> e)
			: mode{ m }, refType{ r }, initExpressions{ std::move(e) } {}

		Element(ElementMode m, ValType r, u32 ti, BufferSlice to, std::vector<BufferSlice> e)
			: mode{ m }, refType{ r }, tablePosition{ {ti, to} }, initExpressions{ std::move(e) } {}

		void print(std::ostream& out);

	private:
		u32 tableIndex() const {
			return tablePosition.has_value() ? tablePosition->tableIndex : 0;
		}

		struct TablePosition {
			u32 tableIndex;
			BufferSlice tableOffset;
		};

		ElementMode mode;
		ValType refType;
		std::optional<TablePosition> tablePosition;
		std::variant<std::vector<u32>, std::vector<BufferSlice>> initExpressions;
	};

	class FunctionCode {
	public:
		struct CompressedLocalTypes {
			u32 count;
			ValType type;
		};

		FunctionCode(BufferSlice c, std::vector<CompressedLocalTypes> l)
			: code{ std::move(c) }, compressedLocalTypes{ std::move(l) } {}

		void print(std::ostream& out);
		void printBody(std::ostream& out);

	private:
		BufferSlice code;
		std::vector<CompressedLocalTypes> compressedLocalTypes;
	};

	class ModuleParser {
	public:
		
		using NameMap = std::unordered_map<u32, std::string>;
		using IndirectNameMap= std::unordered_map<u32, NameMap>;

		Module parse(Buffer, std::string);

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
		BufferSlice parseInitExpression();
		Export parseExport();
		Element parseElement();
		FunctionCode parseFunctionCode();

		std::vector<BufferSlice> parseInitExpressionVector();
		std::vector<u32> parseU32Vector();

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

		std::string mName;
		NameMap functionNames;
		IndirectNameMap functionLocalNames;
	};
}
