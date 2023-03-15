#include <cassert>
#include <iostream>

#include "module.h"
#include "instruction.h"
#include "error.h"

using namespace WASM;

static void printInitExpression(std::ostream& out, const BufferSlice& initExpression) {
	out << "[" << std::hex;

	for (auto byte : initExpression) {
		out << " " << (byte <= 0xF ? "0" : "") << (int)byte;
	}

	out << " ]" << std::dec;
}

Module ModuleParser::parse(Buffer buffer, std::string modulePath)
{
	path = std::move(modulePath);
	data = std::move(buffer);
	it = data.iterator();

	parseHeader();

	while (hasNext()) {
		parseSection();
	}

	for (auto& f : functionCodes) {
		std::cout << "=> Function:\n";
		f.printBody(std::cout);
	}

	return Module{ std::move(data), std::move(path) };
}

std::string ModuleParser::parseNameString()
{
	auto nameLength = nextU32();
	auto nameSlice = nextSliceOf(nameLength);
	return nameSlice.toString();
}

void ModuleParser::parseHeader()
{
	if (!hasNext()) {
		throw std::runtime_error{ "Expected module header is too short" };
	}

	auto magicNumber = nextBigEndianU32();
	auto versionNumber = nextBigEndianU32();

	if (magicNumber != 0x0061736d) {
		throw std::runtime_error{ "Invalid module header magic number" };
	}

	if (versionNumber != 0x01000000) {
		throw std::runtime_error{ "Invalid module header version number" };
	}
}

void ModuleParser::parseSection()
{
	if (!hasNext()) {
		throw std::runtime_error{ "Expected section type byte" };
	}

	auto type = SectionType::fromInt(nextU8());
	auto length = nextU32();

	auto oldPos = it;
	switch (type) {
	case SectionType::Custom: parseCustomSection(length); break;
	case SectionType::Type: parseTypeSection(); break;
	case SectionType::Function: parseFunctionSection(); break;
	case SectionType::Table: parseTableSection(); break;
	case SectionType::Memory: parseMemorySection(); break;
	case SectionType::Global: parseGlobalSection(); break;
	case SectionType::Export: parseExportSection(); break;
	case SectionType::Start: parseStartSection(); break;
	case SectionType::Element: parseElementSection(); break;
	case SectionType::Code: parseCodeSection(); break;
	default:
		std::cout << "Section type not recognized '" << type.name() << "'. skipping " << length << " bytes" << std::endl;
		it += length;
	}

	assert(it == oldPos + length);
}

void ModuleParser::parseCustomSection(u32 length)
{
	if (!hasNext(length)) {
		throw std::runtime_error{ "Custom section is longer than available data" };
	}

	auto endPos = it+ length;
	auto name = parseNameString();
	if (name == "name") {
		parseNameSection(endPos);
		return;
	}

	auto dataSlice = nextSliceTo( endPos );

	std::cout << "-> Parsed custom section '" << name << "' containing " << dataSlice.size() << " bytes" << std::endl;
	customSections.insert( std::make_pair(std::move(name), dataSlice) );
}

void ModuleParser::parseNameSection(BufferIterator endPos)
{
	std::cout << "-> Parsed custom section 'name'" << std::endl;

	std::optional<NameSubsectionType> prevSectionType;

	while (it < endPos) {
		auto type = NameSubsectionType::fromInt(nextU8());
		auto length = nextU32();
		
		if (prevSectionType.has_value() && type <= *prevSectionType) {
			throw std::runtime_error{ "Expected name subsection indices in increasing order" };
		}

		auto oldPos = it;
		switch (type) {
		case NameSubsectionType::ModuleName: {
			mName = parseNameString();
			std::cout << "  - Module name: " << mName << std::endl;
			break;
		}
		case NameSubsectionType::FunctionNames: {
			functionNames = parseNameMap();
			std::cout << "  - Function names: " << std::endl;
			for (auto& n : functionNames) {
				std::cout << "    - " << n.first << " -> " << n.second << std::endl;
			}
			break;
		}
		case NameSubsectionType::LocalNames: {
			functionLocalNames = parseIndirectNameMap();
			std::cout << "  - Local names: " << std::endl;
			for (auto& g : functionLocalNames) {
				std::cout << "    - Group: " << g.first << std::endl;
				for (auto& n : g.second) {
					std::cout << "      - " << n.first << " -> " << n.second << std::endl;
				}
			}
			break;
		}
		default:
			std::cout << "  Name subsection type not recognized '" << type.name() << "'. skipping " << length << " bytes" << std::endl;
			it += length;
		}

		prevSectionType = type;
		assert(it == oldPos + length);
	}
}

void ModuleParser::parseTypeSection()
{
	auto numFunctionTypes = nextU32();

	std::cout << "-> Parsed type section containing " << numFunctionTypes << " function types" << std::endl;

	functionTypes.reserve(functionTypes.size()+ numFunctionTypes);
	for (u32 i = 0; i != numFunctionTypes; i++) {
		auto functionType = parseFunctionType();

		std::cout << "  - " << i << " ";
		functionType.print(std::cout);
		std::cout << std::endl;

		functionTypes.emplace_back( std::move(functionType) );
	}
}

void ModuleParser::parseFunctionSection()
{
	auto numFunctions = nextU32();

	std::cout << "-> Parsed function section containing " << numFunctions << " functions" << std::endl;

	functions.reserve(functions.size() + numFunctions);
	for (u32 i = 0; i != numFunctions; i++) {
		auto typeIdx = nextU32();
		functions.push_back( typeIdx );
		std::cout << "  - " << typeIdx << std::endl;
	}
}

void ModuleParser::parseTableSection()
{
	auto numTables = nextU32();

	std::cout << "-> Parsed table section containing " << numTables << " tables" << std::endl;

	tableTypes.reserve(tableTypes.size() + numTables);
	for (u32 i = 0; i != numTables; i++) {
		auto tableType = parseTableType();

		std::cout << "  - ";
		tableType.print(std::cout);
		std::cout << std::endl;

		tableTypes.emplace_back( std::move(tableType) );
	}
}

void ModuleParser::parseMemorySection()
{
	auto numMemories = nextU32();

	std::cout << "-> Parsed memory section containing " << numMemories << " memories" << std::endl;

	memoryTypes.reserve(memoryTypes.size() + numMemories);
	for (u32 i = 0; i != numMemories; i++) {
		auto memoryType = parseMemoryType();

		std::cout << "  - ";
		memoryType.print(std::cout);
		std::cout << std::endl;

		memoryTypes.emplace_back(std::move(memoryType));
	}
}

void ModuleParser::parseGlobalSection()
{
	auto numGlobals = nextU32();
	
	std::cout << "-> Parsed global section containing " << numGlobals << " globals" << std::endl;

	globals.reserve(globals.size() + numGlobals);
	for (u32 i = 0; i != numGlobals; i++) {
		auto global = parseGlobal();

		std::cout << "  - ";
		global.print(std::cout);
		std::cout << std::endl;

		globals.emplace_back(std::move(global));
	}
}

void ModuleParser::parseExportSection()
{
	auto numExports = nextU32();

	std::cout << "-> Parsed export section containing " << numExports << " exports" << std::endl;

	exports.reserve(exports.size() + numExports);
	for (u32 i = 0; i != numExports; i++) {
		auto exp = parseExport();

		std::cout << "  - ";
		exp.print(std::cout);
		std::cout << std::endl;

		exports.emplace_back(std::move(exp));
	}
}

void ModuleParser::parseStartSection()
{
	auto startFunctionIndex = nextU32();
	startFunction.emplace(startFunctionIndex);

	std::cout << "-> Parsed start section containing start function index " << startFunctionIndex << std::endl;
}

void ModuleParser::parseElementSection()
{
	auto numElements = nextU32();

	std::cout << "-> Parsed element section containing " << numElements << " elements" << std::endl;

	elements.reserve(elements.size() + numElements);
	for (u32 i = 0; i != numElements; i++) {
		auto element = parseElement();

		std::cout << "  - ";
		element.print(std::cout);
		std::cout << std::endl;

		elements.emplace_back(std::move(element));
	}
}

void ModuleParser::parseCodeSection()
{
	auto numFunctionCodes = nextU32();

	std::cout << "-> Parsed code section containing " << numFunctionCodes << " function code items" << std::endl;

	functionCodes.reserve(functionCodes.size() + numFunctionCodes);
	for (u32 i = 0; i != numFunctionCodes; i++) {
		auto code= parseFunctionCode();

		std::cout << "  - ";
		code.print(std::cout);
		std::cout << std::endl;

		functionCodes.emplace_back(std::move(code));
	}
}

ModuleParser::NameMap ModuleParser::parseNameMap()
{
	NameMap nameMap;

	auto numNameAssoc = nextU32();
	nameMap.reserve(numNameAssoc);

	u32 prevNameIdx = 0;

	for (u32 i = 0; i != numNameAssoc; i++) {
		auto nameIdx = nextU32();
		auto name = parseNameString();

		if (i!= 0 && nameIdx <= prevNameIdx) {
			throw std::runtime_error{ "Expected name indices in increasing order for name map." };
		}

		nameMap.emplace( nameIdx, std::move(name) );
		prevNameIdx = nameIdx;
	}

	return nameMap;
}

ModuleParser::IndirectNameMap ModuleParser::parseIndirectNameMap()
{
	IndirectNameMap indirectMap;

	auto numGroups = nextU32();
	indirectMap.reserve(numGroups);

	u32 prevGroupIdx = 0;

	for (u32 i = 0; i != numGroups; i++) {
		auto groupIdx = nextU32();
		auto nameMap = parseNameMap();

		if (i!= 0 && groupIdx <= prevGroupIdx) {
			throw std::runtime_error{ "Expected group indices in increasing order for indirect name map." };
		}

		indirectMap.emplace(groupIdx, std::move(nameMap));
		prevGroupIdx= groupIdx;
	}

	return indirectMap;
}

FunctionType ModuleParser::parseFunctionType()
{
	assertU8(0x60);
	auto parameters = parseResultTypeVector();
	auto results = parseResultTypeVector();
	
	return { std::move(parameters), std::move(results) };
}

std::vector<ValType> ModuleParser::parseResultTypeVector()
{
	auto resultNum = nextU32();
	std::vector<ValType> results;
	results.reserve(resultNum);
	for (u32 i = 0; i != resultNum; i++) {
		auto valType = ValType::fromInt(nextU8());
		if (!valType.isValid()) {
			throw std::runtime_error{ "Found invalid val type while parsing result type vector" };
		}
		results.push_back( valType );
	}

	return results;
}

TableType ModuleParser::parseTableType()
{
	if (!hasNext(3)) {
		throw std::runtime_error{ "Not enough bytes to parse table type" };
	}

	auto elementRefType = ValType::fromInt(nextU8());
	if (!elementRefType.isReference()) {
		throw std::runtime_error{ "Expected reference val type for table element type" };
	}

	auto limits = parseLimits();
	return { elementRefType, limits };
}

MemoryType ModuleParser::parseMemoryType()
{
	if (!hasNext()) {
		throw std::runtime_error{ "Not enough bytes to parse memory type" };
	}
	
	return { parseLimits()};
}

Global ModuleParser::parseGlobal()
{
	if (!hasNext(3)) {
		throw std::runtime_error{ "Not enough bytes to parse global" };
	}

	auto valType = ValType::fromInt(nextU8());
	if (!valType.isValid()) {
		throw std::runtime_error{ "Invalid valtype for global" };
	}

	bool isMutable = false;
	auto isMutableFlag = nextU8();
	if (isMutableFlag <= 0x01) {
		isMutable = static_cast<bool>(isMutableFlag);
	}
	else {
		throw std::runtime_error{ "Invalid mutability flag for global. Expected 0x00 or 0x01" };
	}

	auto initExpressionCode = parseInitExpression();

	return { valType, isMutable, initExpressionCode };
}

Limits ModuleParser::parseLimits()
{
	auto hasMaximum = nextU8();
	if (hasMaximum == 0x00) {
		auto min = nextU32();
		return { min };
	}
	else if (hasMaximum == 0x01) {
		auto min = nextU32();
		auto max = nextU32();
		return { min, max };
	}
	else {
		throw std::runtime_error{ "Invalid limits format. Expected 0x00 or 0x01" };
	}
}

BufferSlice ModuleParser::parseInitExpression()
{
	// FIXME: This should check for well formed-ness of open and closed blocks
	// instead of stopping at the first 0x0B that is found
	auto beginPos = it;
	while (hasNext()) {
		if (nextU8() == 0x0B) {
			return sliceFrom(beginPos);
		}
	}

	throw std::runtime_error{ "Unexpected end of module while parsing init expression" };
}

std::vector<BufferSlice> ModuleParser::parseInitExpressionVector()
{
	auto numExp = nextU32();
	std::vector<BufferSlice> exps;
	exps.reserve(numExp);

	for (u32 i = 0; i != numExp; i++) {
		exps.emplace_back(parseInitExpression());
	}

	return exps;
}

std::vector<u32> ModuleParser::parseU32Vector()
{
	auto numExp = nextU32();
	std::vector<u32> ints;
	ints.reserve(numExp);

	for (u32 i = 0; i != numExp; i++) {
		ints.emplace_back(nextU32());
	}

	return ints;
}

Export ModuleParser::parseExport()
{
	if (!hasNext(3)) {
		throw std::runtime_error{ "Not enough bytes to parse export" };
	}
	
	auto name = parseNameString();
	auto exportType = ExportType::fromInt(nextU8());
	auto index = nextU32();

	return { std::move(name), exportType, index };
}

Element ModuleParser::parseElement()
{
	const auto parseElementKind = [this]() {
		if (nextU8() != 0x00) {
			throw std::runtime_error{ "Only element kind 'function reference' is supported" };
		}
	};

	const auto parseReferenceType = [this]() {
		auto refType = ValType::fromInt(nextU8());
		if (!refType.isReference()) {
			throw std::runtime_error{ "Expected reference type for element" };
		}
		return refType;
	};

	// Bit 0 -> is declarative or passive
	// Bit 1 -> has explicit table index  |  is declarative
	// Bit 2 -> has element type and element expression

	// 0 000          expr          vec(funcidx)   -> active
	// 1 001               elemkind vec(funcidx)   -> passive
	// 2 010 tableidx expr elemkind vec(funcidx)   -> active
	// 3 011               elemkind vec(funcidx)   -> declarative
	// 4 100          expr          vec(expr)      -> active
	// 5 101               reftype  vec(expr)      -> passive
	// 6 110 tableidx expr reftype  vec(expr)      -> active
	// 7 111               reftype  vec(expr)      -> declarative

	auto bitField = nextU32();
	switch (bitField) {
	case 0:
	{
		auto tableOffset = parseInitExpression();
		auto functions = parseU32Vector();
		return {ElementMode::Active, ValType::FuncRef, 0, tableOffset, std::move(functions)};
	}
	case 1:
	{
		parseElementKind();
		auto functions = parseU32Vector();
		return {ElementMode::Passive, ValType::FuncRef, std::move(functions)};
	}
	case 2:
	{
		auto tableIdx = nextU32();
		auto tableOffset = parseInitExpression();
		parseElementKind();
		auto functions = parseU32Vector();
		return {ElementMode::Active, ValType::FuncRef, tableIdx, tableOffset, std::move(functions)};
	}
	case 3:
	{
		parseElementKind();
		auto functions = parseU32Vector();
		return {ElementMode::Declarative, ValType::FuncRef, std::move(functions)};
	}
	case 4:
	{
		auto tableOffset = parseInitExpression();
		auto exprs = parseInitExpressionVector();
		return {ElementMode::Active, ValType::FuncRef, 0, tableOffset, std::move(exprs)};
	}
	case 5:
	{
		auto refType = parseReferenceType();
		auto exprs = parseInitExpressionVector();
		return {ElementMode::Passive, refType, std::move(exprs)};
	}
	case 6:
	{
		auto tableIdx = nextU32();
		auto tableOffset = parseInitExpression();
		auto refType = parseReferenceType();
		auto exprs = parseInitExpressionVector();
		return {ElementMode::Active, refType, tableIdx, tableOffset, std::move(exprs)};
	}
	case 7:
	{
		auto refType = parseReferenceType();
		auto exprs = parseInitExpressionVector();
		return {ElementMode::Declarative, refType, std::move(exprs)};
	}
	default:
		throw std::runtime_error{ "Invalid element bit field" };
	}
}

FunctionCode ModuleParser::parseFunctionCode()
{
	auto byteCount = nextU32();
	auto posBeforeLocals = it;
	auto numLocals = nextU32();
	
	std::vector<FunctionCode::CompressedLocalTypes> locals;
	for (u32 i = 0; i != numLocals; i++) {
		auto localCount = nextU32();
		auto localType = ValType::fromInt(nextU8());
		locals.emplace_back(localCount, localType);
	}

	auto codeSlice = nextSliceTo(posBeforeLocals + byteCount);
	if (codeSlice.isEmpty()) {
		throw std::runtime_error{ "Invalid funcion code item. Empty expression" };
	}
		
	if (codeSlice.last() != 0x0B) {
		throw std::runtime_error{ "Invalid funcion code item. Expected 0x0B at end of expression" };
	}

	return { codeSlice, std::move(locals) };
}

const char* SectionType::name() const
{
	switch (value) {
	case Custom: return "Custom";
	case Type: return "Type";
	case Import: return "Import";
	case Function: return "Function";
	case Table: return "Table";
	case Memory: return "Memory";
	case Global: return "Global";
	case Export: return "Export";
	case Start: return "Start";
	case Element: return "Element";
	case Code: return "Code";
	case Data: return "Data";
	case DataCount: return "DataCount";
	default: return "<unknwon section type>";
	};
}

bool ValType::isNumber() const
{
	switch (value) {
	case I32:
	case I64:
	case F32:
	case F64:
		return true;
	default:
		return false;
	}
}

bool ValType::isVector() const
{
	return value == V128;
}

bool ValType::isReference() const
{
	return value == FuncRef || value == ExternRef;
}

bool ValType::isValid() const
{
	switch (value) {
	case I32:
	case I64:
	case F32:
	case F64:
	case V128:
	case FuncRef:
	case ExternRef:
		return true;
	default:
		return false;
	}
}

const char* ValType::name() const
{
	switch (value) {
		case I32: return "I32";
		case I64: return "I64";
		case F32: return "F32";
		case F64: return "F64";
		case V128: return "V128";
		case FuncRef: return "FuncRef";
		case ExternRef: return "ExternRef";
		default: return "<unknown val type>";
	}
}

void FunctionType::print(std::ostream& out)
{
	out << "Function: ";

	for (auto& param : parameters) {
		out << param.name() << ' ';
	}

	if (parameters.empty()) {
		out << "<none> ";
	}

	out << "-> ";

	for (auto& result : results) {
		out << result.name() << ' ';
	}

	if (results.empty()) {
		out << "<none>";
	}
}

void Limits::print(std::ostream& out)
{
	out << '(' << min;
	if (max.has_value()) {
		out << ", " << *max;
	}
	out << ')';
}

void TableType::print(std::ostream& out)
{
	out << "Table: " << elementReferenceType.name() << ' ';
	limits.print(out);
}

void Global::print(std::ostream& out)
{
	out << "Global: " << (isMutable ? "mutable " : "const ") << type.name() << " ";
	printInitExpression(out, initExpression);
}

const char* ExportType::name() const
{
	switch (value) {
		case FunctionIndex: return "FunctionIndex";
		case TableIndex: return "TableIndex";
		case MemoryIndex: return "MemoryIndex";
		case GlobalIndex: return "GlobalIndex";
		default: return "<unknown export type>";
	}
}

void Export::print(std::ostream& out)
{
	out << "Export: '" << name << "' " << exportType.name() << " " << index;
}

const char* ElementMode::name() const
{
	switch (value) {
		case Passive: return "Passive";
		case Active: return "Active";
		case Declarative: return "Declarative";
		default: return "<unknown element mode>";
	}
}

void Element::print(std::ostream& out)
{
	out << "Element: " << mode.name() << " table: " << tableIndex();
	if (tablePosition) {
		out << " offset: ";
		printInitExpression(out, tablePosition->tableOffset);
	}

	if (initExpressions.index() == 0) {
		for (auto func : std::get<0>(initExpressions)) {
			out << std::endl << "    - func idx " << func;
		}
	}
	else {
		for (auto& expr : std::get<1>(initExpressions)) {
			out << std::endl << "    - expr ";
			printInitExpression(out, expr);
		}
	}
}

void FunctionCode::print(std::ostream& out)
{
	out << "Function code: ";

	for (auto& types : compressedLocalTypes) {
		out << "(" << types.count << "x " << types.type.name() << ") ";
	}

	out << std::endl << "    Code: ";
	printInitExpression(out, code);
}

void FunctionCode::printBody(std::ostream& out)
{
	auto it = code.iterator();
	while (it.hasNext()) {
		auto instruction= Instruction::fromWASMBytes(it);
		out << "  - ";
		instruction.print(out, code);
		out << std::endl;
	}
}

const char* WASM::NameSubsectionType::name() const
{
	switch (value) {
		case ModuleName: return "ModuleName";
		case FunctionNames: return "FunctionNames";
		case LocalNames: return "LocalNames";
		case TypeNames: return "TypeNames";
		case TableNames: return "TableNames";
		case MemoryNames: return "MemoryNames";
		case GlobalNames: return "GlobalNames";
		case ElementSegmentNames: return "ElementSegmentNames";
		case DataSegmentNames: return "DataSegmentNames";
		default: return "<unkown name subsection type>";
	}
}
