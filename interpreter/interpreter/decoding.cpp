#include <cassert>
#include <iostream>

#include "module.h"
#include "instruction.h"
#include "error.h"

using namespace WASM;

void ModuleParser::parse(Buffer buffer, std::string modulePath)
{
	clear();

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
	std::vector<BytecodeFunction> bytecodeFunctions;
	bytecodeFunctions.reserve(functions.size());

	for (u32 i = 0; i != functions.size(); i++) {
		auto functionIdx = i + importedFunctions.size();
		auto typeIdx = functions[i];
		assert(typeIdx < functionTypes.size());
		auto& funcType = functionTypes[typeIdx];
		auto& funcCode = functionCodes[i];
		bytecodeFunctions.emplace_back(functionIdx, typeIdx, funcType, std::move(funcCode));
	}

	std::vector<FunctionTable> functionTables;
	functionTables.reserve(tableTypes.size());

	for (u32 i = 0; i != tableTypes.size(); i++) {
		auto tableIdx = i + importedTableTypes.size();
		auto& tableType = tableTypes[i];
		functionTables.emplace_back(tableIdx, tableType);
	}

	std::vector<DecodedElement> decodedElements;
	decodedElements.reserve(elements.size());

	for (u32 i = 0; i != elements.size(); i++) {
		decodedElements.emplace_back(elements[i].decode(i));
		// decodedElements.back().initTableIfActive( functionTables );
	}

	std::optional<Memory> memoryInstance;
	if (!memoryTypes.empty()) {
		memoryInstance.emplace(0, memoryTypes[0].limits());
	}

	u32 num32BitGlobals= 0;
	u32 num64BitGlobals= 0;
	for (auto& global : globals) {
		auto size = global.valType().sizeInBytes();
		if (size == 4) {
			global.setIndexInTypedStorageArray(num32BitGlobals++);
		}
		else if (size == 8) {
			global.setIndexInTypedStorageArray(num64BitGlobals++);
		}
		else {
			throw ValidationError{ path, "Only globals with 32bits and 64bits are supported" };
		}
	}

	std::vector<Global<u32>> globals32bit;
	globals32bit.reserve(num32BitGlobals);
	globals32bit.insert(globals32bit.end(), num32BitGlobals, {});

	std::vector<Global<u64>> globals64bit;
	globals64bit.reserve(num64BitGlobals);
	globals64bit.insert(globals64bit.end(), num64BitGlobals, {});

	ExportTable exportTable;
	exportTable.reserve(exports.size());

	for (auto& exp : exports) {
		exportTable.emplace( exp.moveName(), exp.toItem());
	}

	std::optional<MemoryImport> memoryImport;
	if (!importedMemoryTypes.empty()) {
		memoryImport.emplace(std::move(importedMemoryTypes[0]));
	}

	// FIXME: Just use the path as name for now
	if (mName.empty()) {
		mName = path;
	}

	return Module{
		std::move(data),
		std::move(path),
		std::move(mName),
		std::move(functionTypes),
		std::move(bytecodeFunctions),
		std::move(functionTables),
		std::move(memoryInstance),
		std::move(exportTable),
		std::move(globals),
		std::move(globals32bit),
		std::move(globals64bit),
		// Imports
		std::move(importedFunctions),
		std::move(importedTableTypes),
		std::move(memoryImport),
		std::move(importedGlobalTypes),
		std::move(functionNames)
	};
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
	case SectionType::Import: parseImportSection(); break;
	case SectionType::Function: parseFunctionSection(); break;
	case SectionType::Table: parseTableSection(); break;
	case SectionType::Memory: parseMemorySection(); break;
	case SectionType::GlobalType: parseGlobalSection(); break;
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

		std::cout << "  - " << i << " ";
		code.print(std::cout);
		std::cout << std::endl;

		functionCodes.emplace_back(std::move(code));
	}
}

void ModuleParser::parseImportSection()
{
	auto numImports = nextU32();
	
	std::cout << "-> Parsed import section containing " << numImports << " import items" << std::endl;

	for (u32 i = 0; i != numImports; i++) {
		auto moduleName = parseNameString();
		auto itemName = parseNameString();

		auto importType = ImportType::fromInt(nextU8());
		std::cout << "  - " << importType.name() << ": " << moduleName << " :: " << itemName << std::endl;

		switch (importType) {
		case ImportType::FunctionImport: {
			auto funcIdx = nextU32();
			importedFunctions.emplace_back(FunctionImport{ std::move(moduleName), std::move(itemName), funcIdx });
			break;
		}
		case ImportType::TableImport: {
			auto tableType= parseTableType();
			importedTableTypes.emplace_back(TableImport{ std::move(moduleName), std::move(itemName), tableType });
			break;
		}
		case ImportType::MemoryImport: {
			auto memoryType = parseMemoryType();
			importedMemoryTypes.emplace_back(MemoryImport{ std::move(moduleName), std::move(itemName), memoryType });
			break;
		}
		case ImportType::GlobalImport: {
			auto globalType = parseGlobalType();
			importedGlobalTypes.emplace_back(GlobalImport{ std::move(moduleName), std::move(itemName), globalType });
			break;
		}
		default:
			assert(false);
		}
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

GlobalType ModuleParser::parseGlobalType()
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

	return { valType, isMutable };
}

DeclaredGlobal ModuleParser::parseGlobal()
{
	auto globalType = parseGlobalType();
	auto initExpressionCode = parseInitExpression();

	return { globalType, initExpressionCode };
}

Limits ModuleParser::parseLimits()
{
	auto hasMaximum = nextU8();
	if (hasMaximum == 0x00) {
		auto mMin = nextU32();
		return { mMin };
	}
	else if (hasMaximum == 0x01) {
		auto mMin = nextU32();
		auto mMax = nextU32();
		return { mMin, mMax };
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
	
	std::vector<CompressedLocalTypes> locals;
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

bool FunctionType::returnsVoid() const
{
	return mResults.empty();
}

bool FunctionType::takesVoidReturnsVoid() const
{
	return mParameters.empty() && mResults.empty();
}

u32 FunctionType::parameterStackSectionSizeInBytes() const
{
	if (requiredParameterStackBytes.has_value()) {
		return *requiredParameterStackBytes;
	}

	u32 numBytesParameters = 0;
	for (auto valType : mParameters) {
		numBytesParameters+= valType.sizeInBytes();
	}
	requiredParameterStackBytes = numBytesParameters;

	return numBytesParameters;
}

u32 FunctionType::resultStackSectionSizeInBytes() const
{
	if (requiredResultStackBytes.has_value()) {
		return *requiredResultStackBytes;
	}

	u32 numBytesResults = 0;
	for (auto valType : mResults) {
		numBytesResults += valType.sizeInBytes();
	}
	requiredResultStackBytes = numBytesResults;

	return numBytesResults;
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
	out << '(' << mMin;
	if (mMax.has_value()) {
		out << ", " << *mMax;
	}
	out << ')';
}

bool Limits::isValid(u32 range) const
{
	if (mMin > range) {
		return false;
	}

	if (mMax.has_value()) {
		return *mMax <= range && mMin <= *mMax;
	}

	return true;
}

void TableType::print(std::ostream& out) const
{
	out << "Table: " << mElementReferenceType.name() << ' ';
	mLimits.print(out);
}

void DeclaredGlobal::setIndexInTypedStorageArray(u32 idx)
{
	assert(!mIndexInTypedStorageArray.has_value());
	mIndexInTypedStorageArray = idx;
}

void DeclaredGlobal::print(std::ostream& out) const
{
	out << "DeclaredGlobal: " << (mType.isMutable() ? "mutable " : "const ") << mType.valType().name() << " ";
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

DecodedElement Element::decode(u32 index)
{
	std::vector<u32> functionIndices;
	if (mInitExpressions.index() == 1) {
		auto& expressions = std::get<1>(mInitExpressions);
		functionIndices.reserve(expressions.size());
		for (auto& expr : expressions) {
			// Do not allow null references
			auto funcIndex = expr.constantFuncRefAsIndex();
			assert(funcIndex.has_value());

			functionIndices.emplace_back(*funcIndex);
		}
	}
	else {
		functionIndices = std::move(std::get<0>(mInitExpressions));
	}

	auto tableOffset = mTablePosition.has_value() ? mTablePosition->tableOffset.constantI32() : 0;

	return {
		index,
		mMode,
		refType,
		tableIndex(),
		(u32)tableOffset,
		std::move(functionIndices)
	};
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

i32 Expression::constantI32() const
{
	assert(mInstructions.size() > 0);
	return mInstructions.front().asI32Constant();
}

std::optional<u32> Expression::constantFuncRefAsIndex() const
{
	assert(mInstructions.size() > 0);
	return mInstructions.front().asReferenceIndex();
}

u64 Expression::constantUntypedValue() const
{
	assert(mInstructions.size() > 0);
	assert(false);
	return 0;
}

void ModuleValidator::validate(const ParsingState& parser)
{
	parsingState = &parser;

	// Under module context C

	if (s().functions.size() != s().functionCodes.size()) {
		throwValidationError("Parsed different number of function declarations than function codes");
	}

	if (s().memoryTypes.size() + s().importedMemoryTypes.size() > 1) {
		throwValidationError("Cannot define or import more than one memory");
	}

	for (u32 i = 0; i != s().functions.size(); i++) {
		validateFunction(i);
	}

	if (s().startFunction.has_value()) {
		validateStartFunction(*s().startFunction);
	}

	validateImports();

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
		typeIdx= s().importedFunctions[funcIdx].functionTypeIndex;
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
	
	// Validation of the actual function code happens in the compiler

	auto& type = s().functionTypes[typeIdx];
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

void ModuleValidator::validateGlobal(const DeclaredGlobal& global)
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

void ModuleValidator::validateImports()
{
	std::cout << "# Validate imports:" << std::endl;
	// TODO: Validate function imports
	
	// Validate table imports
	for (auto& tableType : s().importedTableTypes) {
		validateTableType(tableType.tableType);
	}
	
	// Validate memory imports
	for (auto& memType : s().importedMemoryTypes) {
		validateMemoryType(memType.memoryType);
	}
	
	// Validate global imports -> global types are valid
	std::cout << "# Done validating imports" << std::endl;
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
		if (ins.globalIndex() >= s().importedGlobalTypes.size()) {
			throwValidationError("Init expression references invalid global index");
		}
	}

	std::cout << "Validated global" << std::endl;
}

void ModuleValidator::throwValidationError(const char* msg) const
{
	throw ValidationError{ s().path, msg };
}

void ParsingState::clear()
{
	path.clear();
	data = {};
	it = {};
	customSections.clear();
	functionTypes.clear();
	functions.clear();
	tableTypes.clear();;
	memoryTypes.clear();
	globals.clear();
	exports.clear();
	startFunction.reset();
	elements.clear();
	functionCodes.clear();
	importedFunctions.clear();
	importedTableTypes.clear();
	importedMemoryTypes.clear();
	importedGlobalTypes.clear();
	mName.clear();
	functionNames.clear();
	functionLocalNames.clear();
}

Nullable<const GlobalBase> GlobalImport::getBase() const
{
	if (globalType.valType().sizeInBytes() == 4) {
		auto& ptr= *resolvedGlobal32;
		return ptr;
	}

	auto& ptr = *resolvedGlobal64;
	return ptr;
}
