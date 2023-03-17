#include <cassert>
#include <iostream>

#include "module.h"
#include "instruction.h"
#include "error.h"

using namespace WASM;

void ModuleParser::parse(Buffer buffer, std::string modulePath)
{
	path = std::move(modulePath);
	data = std::move(buffer);
	it = data.iterator();

	parseHeader();

	while (hasNext()) {
		parseSection();
	}

	for (auto& f : functionCodes) {
		std::cout << "=> Function:";
		f.printBody(std::cout);
		std::cout << std::endl;
	}
}

Module ModuleParser::toModule()
{
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
		throwParsingError("Expected module header is too short");
	}

	auto magicNumber = nextBigEndianU32();
	auto versionNumber = nextBigEndianU32();

	if (magicNumber != 0x0061736d) {
		throwParsingError("Invalid module header magic number");
	}

	if (versionNumber != 0x01000000) {
		throwParsingError("Invalid module header version number");
	}
}

void ModuleParser::parseSection()
{
	if (!hasNext()) {
		throwParsingError("Expected section type byte");
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
		throwParsingError("Custom section is longer than available data");
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
			throwParsingError("Expected name subsection indices in increasing order");
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
			throwParsingError("Expected name indices in increasing order for name map.");
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
			throwParsingError("Expected group indices in increasing order for indirect name map.");
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
			throwParsingError("Found invalid val type while parsing result type vector");
		}
		results.push_back( valType );
	}

	return results;
}

TableType ModuleParser::parseTableType()
{
	if (!hasNext(3)) {
		throwParsingError("Not enough bytes to parse table type");
	}

	auto elementRefType = ValType::fromInt(nextU8());
	if (!elementRefType.isReference()) {
		throwParsingError("Expected reference val type for table element type");
	}

	auto limits = parseLimits();
	return { elementRefType, limits };
}

MemoryType ModuleParser::parseMemoryType()
{
	if (!hasNext()) {
		throwParsingError("Not enough bytes to parse memory type");
	}
	
	return { parseLimits()};
}

Global ModuleParser::parseGlobal()
{
	if (!hasNext(3)) {
		throwParsingError("Not enough bytes to parse global");
	}

	auto valType = ValType::fromInt(nextU8());
	if (!valType.isValid()) {
		throwParsingError("Invalid valtype for global");
	}

	bool isMutable = false;
	auto isMutableFlag = nextU8();
	if (isMutableFlag <= 0x01) {
		isMutable = static_cast<bool>(isMutableFlag);
	}
	else {
		throwParsingError("Invalid mutability flag for global. Expected 0x00 or 0x01");
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
		throwParsingError("Invalid limits format. Expected 0x00 or 0x01");
	}
}

Expression ModuleParser::parseInitExpression()
{
	std::vector<Instruction> instructions;
	auto beginPos = it;
	while (hasNext()) {
		auto& ins = instructions.emplace_back(Instruction::fromWASMBytes(it));
		if (ins == InstructionType::End) {
			return { sliceFrom(beginPos), std::move(instructions) };
		}
	}

	throwParsingError("Unexpected end of module while parsing init expression");
}

std::vector<Expression> ModuleParser::parseInitExpressionVector()
{
	auto numExp = nextU32();
	std::vector<Expression> exps;
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

void ModuleParser::throwParsingError(const char* msg) const
{
	throw ParsingError{ static_cast<u64>(it.positionPointer() - data.begin()), path, std::string{msg} };
}

Export ModuleParser::parseExport()
{
	if (!hasNext(3)) {
		throwParsingError("Not enough bytes to parse export");
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
			throwParsingError("Only element kind 'function reference' is supported");
		}
	};

	const auto parseReferenceType = [this]() {
		auto refType = ValType::fromInt(nextU8());
		if (!refType.isReference()) {
			throwParsingError("Expected reference type for element");
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
		throwParsingError("Invalid element bit field");
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
		throwParsingError("Invalid funcion code item. Empty expression");
	}
		
	if (codeSlice.last() != 0x0B) {
		throwParsingError("Invalid funcion code item. Expected 0x0B at end of expression");
	}

	std::vector<Instruction> instructions;
	auto codeIt = codeSlice.iterator();
	while (codeIt.hasNext()) {
		instructions.emplace_back(Instruction::fromWASMBytes(codeIt));
	}

	return { Expression{codeSlice, instructions}, std::move(locals) };
}

bool FunctionType::takesVoidReturnsVoid() const
{
	return mParameters.empty() && mResults.empty();
}

void FunctionType::print(std::ostream& out) const
{
	out << "Function: ";

	for (auto& param : mParameters) {
		out << param.name() << ' ';
	}

	if (mParameters.empty()) {
		out << "<none> ";
	}

	out << "-> ";

	for (auto& result : mResults) {
		out << result.name() << ' ';
	}

	if (mResults.empty()) {
		out << "<none>";
	}
}

void Limits::print(std::ostream& out) const
{
	out << '(' << min;
	if (max.has_value()) {
		out << ", " << *max;
	}
	out << ')';
}

bool Limits::isValid(u32 range) const
{
	if (min > range) {
		return false;
	}

	if (max.has_value()) {
		return *max <= range && min <= *max;
	}

	return true;
}

void TableType::print(std::ostream& out) const
{
	out << "Table: " << mElementReferenceType.name() << ' ';
	mLimits.print(out);
}

void Global::print(std::ostream& out) const
{
	out << "Global: " << (mIsMutable ? "mutable " : "const ") << mType.name() << " ";
	mInitExpression.printBytes(out);
}

bool Export::isValid(u32 numFunctions, u32 numTables, u32 numMemories, u32 numGlobals) const
{
	switch (mExportType) {
	case ExportType::FunctionIndex: return mIndex < numFunctions;
	case ExportType::TableIndex: return mIndex < numTables;
	case ExportType::MemoryIndex: return mIndex < numTables;
	case ExportType::GlobalIndex: return mIndex < numGlobals;
	default:
		assert(false);
	}
}

void Export::print(std::ostream& out) const
{
	out << "Export: '" << mName << "' " << mExportType.name() << " " << mIndex;
}

Nullable<const std::vector<Expression>> Element::initExpressions() const
{
	if (mInitExpressions.index() == 1) {
		return { std::get<1>(mInitExpressions) };
	}

	return {};
}

void Element::print(std::ostream& out) const
{
	out << "Element: " << refType.name() << " " << mMode.name() << " table: " << tableIndex();
	if (mTablePosition) {
		out << " offset: ";
		mTablePosition->tableOffset.printBytes(out);
	}

	if (mInitExpressions.index() == 0) {
		for (auto func : std::get<0>(mInitExpressions)) {
			out << std::endl << "    - func idx " << func;
		}
	}
	else {
		for (auto& expr : std::get<1>(mInitExpressions)) {
			out << std::endl << "    - expr ";
			expr.printBytes(out);
		}
	}
}

void FunctionCode::print(std::ostream& out) const
{
	out << "Function code: ";

	for (auto& types : compressedLocalTypes) {
		out << "(" << types.count << "x " << types.type.name() << ") ";
	}

	out << std::endl << "    Code: ";
	code.printBytes(out);
}

void FunctionCode::printBody(std::ostream& out) const
{
	code.print(out);
}

void Expression::printBytes(std::ostream& out) const
{
	mBytes.print(out);
}

void Expression::print(std::ostream& out) const
{
	for (auto& ins : mInstructions) {
		out << "\n  - ";
		ins.print(out, mBytes);
	}
}

void ModuleValidator::validate(const ParsingState& parser)
{
	parsingState = &parser;

	// Under module context C

	if (s().functions.size() != s().functionCodes.size()) {
		throwValidationError("Parsed different number of function declarations than function codes");
	}

	for (u32 i = 0; i != s().functions.size(); i++) {
		validateFunction(i);
	}

	if (s().startFunction.has_value()) {
		validateStartFunction(*s().startFunction);
	}

	// TODO: Validate imports

	for (auto& exp : s().exports) {
		validateExport(exp);
	}

	// Under context C'

	for (auto& table : s().tableTypes) {
		validateTableType(table);
	}

	for (auto& mem : s().memoryTypes) {
		validateMemoryType(mem);
	}

	if (s().memoryTypes.size() > 1) {
		throwValidationError("More than one memory is not allowed");
	}

	for (auto& global : s().globals) {
		validateGlobal(global);
	}

	for (auto& elem : s().elements) {
		validateElementSegment(elem);
	}

	// TODO: Validate data segments

	parsingState = nullptr;
}

const FunctionType& ModuleValidator::functionTypeByIndex(u32 funcIdx)
{
	u32 typeIdx;
	if (funcIdx < s().importedFunctions.size()) {
		typeIdx= s().importedFunctions[funcIdx];
	}

	funcIdx -= s().importedFunctions.size();
	if (funcIdx > s().functions.size()) {
		throwValidationError("Invalid function index");
	}
	
	typeIdx= s().functions[funcIdx];
	if (typeIdx > s().functionTypes.size()) {
		throwValidationError("Function references invalid type index");
	}
	return s().functionTypes[typeIdx];
}

void ModuleValidator::validateFunction(u32 funcNum)
{
	auto typeIdx = s().functions[funcNum];
	if (typeIdx > s().functionTypes.size()) {
		throwValidationError("Function references invalid type index");
	}

	auto& type = s().functionTypes[typeIdx];
	auto& code = s().functionCodes[funcNum];

	setFunctionContext( type, code );
	valueStack.clear();
	controlStack.clear();

	controlStack.emplace_back(InstructionType::NoOperation, BlockTypeIndex{BlockType::TypeIndex, typeIdx}, 0, false);

	for (auto& ins : code) {
		validateOpcode(ins);
	}

	std::cout << "Validated function " << funcNum << " with type ";
	type.print(std::cout);
	std::cout << std::endl;
}

void ModuleValidator::validateTableType(const TableType& t)
{
	constexpr u32 tableRange = 0xFFFFFFFF;
	if (!t.limits().isValid(tableRange)) {
		throwValidationError("Invalid table limits definition");
	}

	std::cout << "Validated table type" << std::endl;
}

void ModuleValidator::validateMemoryType(const MemoryType& m)
{
	constexpr u32 memoryRange = 0xFFFF;
	if (!m.limits().isValid(memoryRange)) {
		throwValidationError("Invalid range limits definition");
	}

	std::cout << "Validated memory type" << std::endl;
}

void ModuleValidator::validateExport(const Export& e)
{
	auto numFunctions = s().functions.size() + s().importedFunctions.size();
	auto numTables = s().tableTypes.size() + s().importedTableTypes.size();
	auto numMemories = s().memoryTypes.size() + s().importedMemoryTypes.size();
	auto numGlobals = s().globals.size() + s().importedGlobalTypes.size();
	if (!e.isValid(numFunctions, numTables, numMemories, numGlobals)) {
		throwValidationError("Export references invalid index");
	}

	// Try insert the name and throw if it already exists
	auto result= exportNames.emplace(e.name());
	if ( !result.second ) {
		throwValidationError("Duplicate export name");
	}

	std::cout << "Validated export '" << e.name() << "'" << std::endl;
}

void ModuleValidator::validateStartFunction(u32 idx)
{
	auto& funcType = functionTypeByIndex(idx);
	if (!funcType.takesVoidReturnsVoid()) {
		throwValidationError("Start function has wrong type");
	}

	std::cout << "Validated start function" << std::endl;
}

void ModuleValidator::validateGlobal(const Global& global)
{
	validateConstantExpression(global.initExpression(), global.valType());
}

void ModuleValidator::validateElementSegment(const Element& elem)
{
	auto initExpressions = elem.initExpressions();
	if (initExpressions.has_value()) {
		for (auto& expr : *initExpressions) {
			validateConstantExpression(expr, elem.valType());
		}
	}
	else {
		if (elem.valType() != ValType::FuncRef) {
			throwValidationError("Element segment cannot be initialized with function references, wrong type.");
		}
	}

	if (elem.mode() == ElementMode::Active) {
		// FIXME: Assume that C.tables[x] means the local module only. 
		// The current context C' does not have any tables defined. Or does it?

		auto tableIdx = elem.tableIndex();
		if (tableIdx >= s().tableTypes.size()) {
			throwValidationError("Element segment references invalid table index");
		}

		auto& table = s().tableTypes[tableIdx];
		if (table.valType() != elem.valType()) {
			throwValidationError("Element segment type missmatch with reference table");
		}

		auto& tablePos = elem.tablePosition();
		assert(tablePos.has_value());
		
		validateConstantExpression(tablePos->tableOffset, ValType::I32);
	}

	std::cout << "Validated element segment" << std::endl;
}

void ModuleValidator::validateConstantExpression(const Expression& exp, ValType expectedType)
{
	// Only instructions that return something on the stack are allowed, and only
	// one result [t] is expected on the stack. Therefore, only one instruction plus
	// 'End' can occur
	if (exp.size() > 2) {
		throwValidationError("Wrong stack type for init expression");
	}

	auto& ins = *exp.begin();
	if (!ins.isConstant() && ins != InstructionType::End) {
		throwValidationError("Non-const instruction in init expression");
	}

	auto resultType = ins.constantType();
	if (resultType.has_value() && *resultType != expectedType) {
		throwValidationError("Constant expression yields unexpected type");
	}

	if (ins == InstructionType::GlobalGet) {
		// TODO: Constant GlobalGet is only allowed for imported globals, which do not exist yet
		assert(false);
	}

	std::cout << "Validated global" << std::endl;
}

void WASM::ModuleValidator::setFunctionContext(const FunctionType& type, const FunctionCode& code)
{
	currentFunctionType = &type;
	currentLocals = &code.locals();
}

void ModuleValidator::pushValue(ValType type)
{
	valueStack.emplace_back(type);
}

ModuleValidator::ValueRecord ModuleValidator::popValue()
{
	if (controlStack.empty()) {
		throwValidationError("Control stack is empty");
	}

	auto& frame = controlStack.back();
	if (valueStack.size() == frame.height && frame.unreachable) {
		return {};
	}

	if (valueStack.size() == frame.height) {
		throwValidationError("Value stack underflows current block height");
	}

	if (valueStack.empty()) {
		throwValidationError("Value stack underflow");
	}

	auto valueTop = valueStack.back();
	valueStack.pop_back();
	return valueTop;
}

ModuleValidator::ValueRecord ModuleValidator::popValue(ValueRecord expected)
{
	auto actual = popValue();
	if (!expected.has_value() || !actual.has_value()) {
		return actual;
	}

	if (*expected == *actual) {
		return actual;
	}

	throwValidationError("Stack types differ");
}

void ModuleValidator::pushValues(const std::vector<ValType>& types)
{
	valueStack.reserve(valueStack.size()+ types.size());
	valueStack.insert(valueStack.end(), types.begin(), types.end());
}

void ModuleValidator::pushValues(const BlockTypeParameters& parameters)
{
	if (parameters.has_value()) {
		if (*parameters >= s().functionTypes.size()) {
			throwValidationError("Block type index references invalid function type");
		}
		auto& type = s().functionTypes[*parameters];
		pushValues(type.parameters());
	}
}

void ModuleValidator::pushValues(const BlockTypeResults& results)
{
	if (results == BlockType::TypeIndex) {
		if (results.index >= s().functionTypes.size()) {
			throwValidationError("Block type index references invalid function type");
		}
		auto& type = s().functionTypes[results.index];
		pushValues(type.results());
		return;
	}

	if (results == BlockType::ValType) {
		auto valType = ValType::fromInt(results.index);
		assert(valType.isValid());
		pushValue(valType);
	}
}

void ModuleValidator::pushValues(const ModuleValidator::LabelTypes& types)
{
	if (types.index() == 0) {
		pushValues(std::get<0>(types));
	}
	else {
		pushValues(std::get<1>(types));
	}
}

void ModuleValidator::resetCachedReturnList(u32 expectedSize)
{
	cachedReturnList.clear();
	cachedReturnList.reserve(expectedSize);
	cachedReturnList.assign(expectedSize, ValueRecord{});
}

ValType WASM::ModuleValidator::localByIndex(u32 idx) const
{
	assert(currentFunctionType && currentLocals);

	auto& params = currentFunctionType->parameters();
	if (idx < params.size()) {
		return params[idx];
	}

	idx -= params.size();
	for (auto& pack : *currentLocals) {
		if (idx < pack.count) {
			return pack.type;
		}

		idx -= pack.count;
	}

	throwValidationError("Local index out of bounds");
}

std::vector<ModuleValidator::ValueRecord>& ModuleValidator::popValues(const std::vector<ModuleValidator::ValueRecord>& expected)
{
	resetCachedReturnList(expected.size());

	// Iterate in reverse
	auto insertIt = cachedReturnList.rend();
	for (auto it = expected.rbegin(); it != expected.rend(); it++) {
		*(insertIt++) = popValue(*it);
	}

	return cachedReturnList;
}

std::vector<ModuleValidator::ValueRecord>& ModuleValidator::popValues(const std::vector<ValType>& expected)
{
	resetCachedReturnList(expected.size());

	// Iterate in reverse
	auto insertIt = cachedReturnList.rbegin();
	for (auto it = expected.rbegin(); it != expected.rend(); it++) {
		*(insertIt++) = popValue(*it);
	}

	return cachedReturnList;
}

std::vector<ModuleValidator::ValueRecord>& ModuleValidator::popValues(const BlockTypeResults& expected) {
	if (expected == BlockType::TypeIndex) {
		if (expected.index >= s().functionTypes.size()) {
			throwValidationError("Block type index references invalid function type");
		}
		auto& type = s().functionTypes[expected.index];
		return popValues(type.results());
	}

	if (expected == BlockType::ValType) {
		resetCachedReturnList(1);
		auto valType = ValType::fromInt(expected.index);
		assert(valType.isValid());
		cachedReturnList[0]= popValue(valType);
		return cachedReturnList;
	}

	resetCachedReturnList(0);
	return cachedReturnList;
}

std::vector<ModuleValidator::ValueRecord>& ModuleValidator::popValues(const BlockTypeParameters& expected) {
	if (expected.has_value()) {
		if (*expected >= s().functionTypes.size()) {
			throwValidationError("Block type index references invalid function type");
		}
		auto& type = s().functionTypes[*expected];
		return popValues(type.parameters());
	}

	resetCachedReturnList(0);
	return cachedReturnList;
}

std::vector<ModuleValidator::ValueRecord>& ModuleValidator::popValues(const ModuleValidator::LabelTypes& types)
{
	if (types.index() == 0) {
		return popValues(std::get<0>(types));
	}

	return popValues(std::get<1>(types));
}

void ModuleValidator::pushControlFrame(InstructionType opCode, BlockTypeIndex blockTypeIndex)
{
	controlStack.emplace_back(opCode, blockTypeIndex, valueStack.size(), false);
	pushValues(blockTypeIndex.parameters());
}

ModuleValidator::ControlFrame ModuleValidator::popControlFrame()
{
	if (controlStack.empty()) {
		throwValidationError("Control stack underflow");
	}

	auto& frame = controlStack.back();
	popValues(frame.blockTypeIndex.results());
	if (valueStack.size() != frame.height) {
		throwValidationError("Value stack height missmatch");
	}

	controlStack.pop_back();
	return frame;
}

void ModuleValidator::setUnreachable()
{
	if (controlStack.empty()) {
		throwValidationError("Control stack underflow");
	}

	auto& frame = controlStack.back();
	valueStack.resize(frame.height);
	frame.unreachable = true;
}

void WASM::ModuleValidator::validateOpcode(Instruction instruction)
{
	auto opCode = instruction.opCode();
	if (opCode.isConstant() && opCode != InstructionType::GlobalGet) {
		auto resultType = opCode.resultType();
		assert(resultType.has_value());
		pushValue(*resultType);
		return;
	}

	if (opCode.isUnary()) {
		auto operandType = opCode.operandType();
		auto resultType = opCode.resultType();
		assert(operandType.has_value() && resultType.has_value());
		popValue(*operandType);
		pushValue(*resultType);
		return;
	}

	if (opCode.isBinary()) {
		auto operandType = opCode.operandType();
		auto resultType = opCode.resultType();
		assert(operandType.has_value() && resultType.has_value());
		popValue(*operandType);
		popValue(*operandType);
		pushValue(*resultType);
		return;
	}

	auto validateBlockTypeInstruction = [&]() {
		auto blockType= instruction.blockTypeIndex();
		popValues(blockType.parameters());
		pushControlFrame(instruction.opCode(), blockType);
	};

	auto validateBranchTypeInstruction = [&]() {		
		auto label = instruction.branchLabel();
		if (label > controlStack.size() || controlStack.empty()) {
			throwValidationError("Branch label underflows control frame stack");
		}

		auto& frame = controlStack[controlStack.size() - label - 1];
		auto labelTypes = frame.labelTypes();
		popValues(labelTypes);

		return labelTypes;
	};

	using IT = InstructionType;
	switch (opCode) {
	case IT::Unreachable:
		setUnreachable();
		return;

	case IT::NoOperation:
		return;

	case IT::Block:
	case IT::Loop:
		validateBlockTypeInstruction();
		return;

	case IT::If:
		popValue(ValType::I32);
		validateBlockTypeInstruction();
		return;

	case IT::Else: {
		auto frame = popControlFrame();
		if (frame.opCode != InstructionType::If) {
			throwValidationError("If block expected before else block");
		}
		pushControlFrame(InstructionType::Else, frame.blockTypeIndex);
		return;
	}

	case IT::End: {
		auto frame = popControlFrame();
		pushValues(frame.blockTypeIndex.results());
		return;
	}

	case IT::Branch:
		validateBranchTypeInstruction();
		setUnreachable();
		return;

	case IT::BranchIf: {
		popValue(ValType::I32);
		auto labelTypes = validateBranchTypeInstruction();
		pushValues(labelTypes);
		return;
	}

	case IT::Return: {
		if (controlStack.empty()) {
			throwValidationError("Control stack underflow during return");
		}
		popValues(controlStack[0].blockTypeIndex.results());
		setUnreachable();
		return;
	}

	// case IT::BranchTable:

	case IT::Call: {
		auto functionIdx = instruction.functionIndex();
		auto& funcType = functionTypeByIndex(functionIdx);
		popValues(funcType.parameters());
		pushValues(funcType.results());
		return;
	}

	case IT::Drop:
		popValue();
		return;

	case IT::LocalGet: {
		auto localType = localByIndex(instruction.localIndex());
		pushValue(localType);
		return;
	}

	case IT::LocalSet: {
		auto localType = localByIndex(instruction.localIndex());
		popValue(localType);
		return;
	}

	}

	std::cerr << "Validation not implemented for instruction '" << opCode.name() << "'!" << std::endl;
	throw std::runtime_error{ "Validation not implemented for instruction" };
}

void ModuleValidator::throwValidationError(const char* msg) const
{
	throw ValidationError{ s().path, msg };
}

std::variant<BlockTypeParameters, BlockTypeResults> ModuleValidator::ControlFrame::labelTypes() const
{
	if(opCode == InstructionType::Loop) {
		return { blockTypeIndex.parameters() };
	}

	return { blockTypeIndex.results() };
}
