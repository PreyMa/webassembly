#include <cassert>
#include <iostream>

#include "interpreter.h"
#include "introspection.h"
#include "error.h"

using namespace WASM;

void ModuleParser::parse(Buffer buffer, std::string modulePath)
{
	clear();
	
	if (introspector.has_value()) {
		introspector->onModuleParsingStart(modulePath);
	}

	path = std::move(modulePath);
	data = std::move(buffer);
	it = data.iterator();

	parseHeader();

	while (hasNext()) {
		parseSection();
	}

	if (introspector.has_value()) {
		introspector->onModuleParsingFinished(functionCodes);
	}
}

Module ModuleParser::toModule()
{
	// Create bytecode function objects
	std::vector<BytecodeFunction> bytecodeFunctions;
	bytecodeFunctions.reserve(functions.size());

	for (u32 i = 0; i != functions.size(); i++) {
		ModuleFunctionIndex functionIdx{ (u32)(i + importedFunctions.size()) };
		auto typeIdx = functions[i];
		assert(typeIdx < functionTypes.size());
		auto& funcType = functionTypes[typeIdx.value];
		auto& funcCode = functionCodes[i];
		bytecodeFunctions.emplace_back(functionIdx, typeIdx, funcType, std::move(funcCode));
	}

	// Create function table objects
	std::vector<FunctionTable> functionTables;
	functionTables.reserve(tableTypes.size());

	for (u32 i = 0; i != tableTypes.size(); i++) {
		ModuleTableIndex tableIdx{ (u32)(i + importedTableTypes.size()) };
		auto& tableType = tableTypes[i];
		functionTables.emplace_back(tableIdx, tableType);
	}

	// Create memory instance if one is defined
	std::optional<Memory> memoryInstance;
	if (!memoryTypes.empty()) {
		memoryInstance.emplace(ModuleMemoryIndex{ 0 }, memoryTypes[0].limits());
	}

	// Count the number of 32bit and 64bit globals, assign relative indices
	// and allocate arrays for them
	u32 num32BitGlobals= 0;
	u32 num64BitGlobals= 0;
	for (auto& global : globals) {
		auto size = global.valType().sizeInBytes();
		if (size == 4) {
			ModuleGlobalTypedArrayIndex idx{ num32BitGlobals++ };
			global.setIndexInTypedStorageArray(idx);
		}
		else if (size == 8) {
			ModuleGlobalTypedArrayIndex idx{ num64BitGlobals++ };
			global.setIndexInTypedStorageArray(idx);
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

	// Create export table object
	ExportTable exportTable;
	exportTable.reserve(exports.size());

	for (auto& exp : exports) {
		exportTable.emplace( exp.moveName(), exp.toItem());
	}

	// Create memory import if one is required
	std::optional<MemoryImport> memoryImport;
	if (!importedMemoryTypes.empty()) {
		memoryImport.emplace(std::move(importedMemoryTypes[0]));
	}

	// FIXME: Just use the path as name for now
	if (mName.empty()) {
		auto begin= path.find_last_of("/\\");
		if (begin == std::string::npos) {
			begin = 0;
		}

		auto end = path.find_first_of('.', begin);
		if (end == std::string::npos) {
			end = path.size();
		}

		mName = path.substr(begin+1, end- begin -1);
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
		std::move(elements),
		std::move(startFunctionIndex),
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
	// Parse a module section starting with a type identifying byte
	// and a u32 length
	// https://webassembly.github.io/spec/core/binary/modules.html#sections

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
		if (introspector.has_value()) {
			introspector->onSkippingUnrecognizedSection(type, length);
		}
		it += length;
	}

	// Check that the whole section was consumed
	assert(it == oldPos + length);
}

void ModuleParser::parseCustomSection(u32 length)
{
	// Custom sections consist of a name and uninterpreted
	// bytes. The number of bytes is the size of the whole
	// section minus the size of the name string. Name sections
	// are special custom sections recognized by their name
	// https://webassembly.github.io/spec/core/binary/modules.html#custom-section

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
	if (introspector.has_value()) {
		introspector->onParsingCustomSection(name, dataSlice);
	}

	customSections.insert( std::make_pair(std::move(name), dataSlice) );
}

void ModuleParser::parseNameSection(BufferIterator endPos)
{
	// Name sections consist of multiple optional subsections which
	// need to appear in order. Each subsection has an identifying byte and
	// a u32 size.
	// https://webassembly.github.io/spec/core/appendix/custom.html#name-section

	std::optional<NameSubsectionType> prevSectionType;

	while (it < endPos) {
		auto type = NameSubsectionType::fromInt(nextU8());
		auto length = nextU32();
		
		// Check order if there was a section parsed already before
		if (prevSectionType.has_value() && type <= *prevSectionType) {
			throwParsingError("Expected name subsection indices in increasing order");
		}

		auto oldPos = it;
		switch (type) {
		case NameSubsectionType::ModuleName: {
			mName = parseNameString();
			break;
		}
		case NameSubsectionType::FunctionNames: {
			functionNames = parseNameMap();
			break;
		}
		case NameSubsectionType::LocalNames: {
			functionLocalNames = parseIndirectNameMap();
			break;
		}
		default:
			if (introspector.has_value()) {
				introspector->onSkippingUnrecognizedNameSubsection(type, length);
			}
			it += length;
		}

		prevSectionType = type;
		assert(it == oldPos + length);
	}

	if (introspector.has_value()) {
		introspector->onParsingNameSection(mName, functionNames, functionLocalNames);
	}
}

void ModuleParser::parseTypeSection()
{
	// The type section consist of a single vector of function types
	// https://webassembly.github.io/spec/core/binary/modules.html#type-section

	auto numFunctionTypes = nextU32();
	functionTypes.reserve(functionTypes.size()+ numFunctionTypes);
	for (u32 i = 0; i != numFunctionTypes; i++) {
		auto functionType = parseFunctionType();
		functionTypes.emplace_back( std::move(functionType) );
	}

	if (introspector.has_value()) {
		introspector->onParsingTypeSection(functionTypes);
	}
}

void ModuleParser::parseFunctionSection()
{
	// The function section consists of a single vector of function indices
	// https://webassembly.github.io/spec/core/binary/modules.html#function-section

	auto numFunctions = nextU32();
	functions.reserve(functions.size() + numFunctions);
	for (u32 i = 0; i != numFunctions; i++) {
		ModuleTypeIndex typeIdx{ nextU32() };
		functions.push_back( typeIdx );
	}

	if (introspector.has_value()) {
		introspector->onParsingFunctionSection(functions);
	}
}

void ModuleParser::parseTableSection()
{
	// The table section consists of a single vector of table types
	// https://webassembly.github.io/spec/core/binary/modules.html#table-section

	auto numTables = nextU32();
	tableTypes.reserve(tableTypes.size() + numTables);
	for (u32 i = 0; i != numTables; i++) {
		auto tableType = parseTableType();
		tableTypes.emplace_back( std::move(tableType) );
	}

	if (introspector.has_value()) {
		introspector->onParsingTableSection(tableTypes);
	}
}

void ModuleParser::parseMemorySection()
{
	// The memory section consists of a single vector of memory types,
	// even though only a single memory per module is supported right now
	// https://webassembly.github.io/spec/core/binary/modules.html#memory-section

	auto numMemories = nextU32();
	memoryTypes.reserve(memoryTypes.size() + numMemories);
	for (u32 i = 0; i != numMemories; i++) {
		auto memoryType = parseMemoryType();
		memoryTypes.emplace_back(std::move(memoryType));
	}

	if (introspector.has_value()) {
		introspector->onParsingMemorySection(memoryTypes);
	}
}

void ModuleParser::parseGlobalSection()
{
	// The global section consists of a single vector of globals
	// https://webassembly.github.io/spec/core/binary/modules.html#global-section
	
	auto numGlobals = nextU32();
	globals.reserve(globals.size() + numGlobals);
	for (u32 i = 0; i != numGlobals; i++) {
		auto global = parseGlobal();
		globals.emplace_back(std::move(global));
	}

	if (introspector.has_value()) {
		introspector->onParsingGlobalSection(globals);
	}
}

void ModuleParser::parseExportSection()
{
	// The export section consists of a single vector of exports
	// https://webassembly.github.io/spec/core/binary/modules.html#export-section

	auto numExports = nextU32();
	exports.reserve(exports.size() + numExports);
	for (u32 i = 0; i != numExports; i++) {
		auto exp = parseExport();
		exports.emplace_back(std::move(exp));
	}

	if (introspector.has_value()) {
		introspector->onParsingExportSection(exports);
	}
}

void ModuleParser::parseStartSection()
{
	// The start section contains a single function index pointing to the
	// module's start function
	// https://webassembly.github.io/spec/core/binary/modules.html#start-section

	ModuleFunctionIndex idx{ nextU32() };
	startFunctionIndex.emplace(idx);

	if (introspector.has_value()) {
		introspector->onParsingStrartSection(idx);
	}
}

void ModuleParser::parseElementSection()
{
	// The element section consists of a single vector of elements
	// https://webassembly.github.io/spec/core/binary/modules.html#element-section

	auto numElements = nextU32();
	elements.reserve(elements.size() + numElements);
	for (u32 i = 0; i != numElements; i++) {
		auto element = parseElement();
		elements.emplace_back(std::move(element));
	}

	if (introspector.has_value()) {
		introspector->onParsingElementSection(elements);
	}
}

void ModuleParser::parseCodeSection()
{
	// The code section consists of a single vector of function code items
	// https://webassembly.github.io/spec/core/binary/modules.html#code-section

	auto numFunctionCodes = nextU32();
	functionCodes.reserve(functionCodes.size() + numFunctionCodes);
	for (u32 i = 0; i != numFunctionCodes; i++) {
		auto code= parseFunctionCode();
		functionCodes.emplace_back(std::move(code));
	}

	if (introspector.has_value()) {
		introspector->onParsingCodeSection(functionCodes);
	}
}

void ModuleParser::parseImportSection()
{
	// The import section consists of a single vector of imports. Each import
	// starts with the name of the module to import from, followed by another name
	// of the item to be imported. An identifying byte describes the type of item
	// to be imported, followed by type specific data: eg. function imports declare
	// a type index and global imports declare a global type
	// https://webassembly.github.io/spec/core/binary/modules.html#import-section

	auto numImports = nextU32();
	for (u32 i = 0; i != numImports; i++) {
		// Get the module name, item name and type common to all imports
		auto moduleName = parseNameString();
		auto itemName = parseNameString();
		auto importType = ImportType::fromInt(nextU8());

		// Parse each import type
		switch (importType) {
		case ImportType::FunctionImport: {
			ModuleTypeIndex funcTypeIdx{ nextU32() };
			importedFunctions.emplace_back(FunctionImport{ std::move(moduleName), std::move(itemName), funcTypeIdx });
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

	if (introspector.has_value()) {
		introspector->onParsingImportSection(importedFunctions, importedTableTypes, importedMemoryTypes, importedGlobalTypes);
	}
}

ModuleParser::NameMap ModuleParser::parseNameMap()
{
	// Name maps are a vector of name associations, which consist of an index
	// and a name each forming pairs. Indices have to appear in order.
	// https://webassembly.github.io/spec/core/appendix/custom.html#name-maps

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
	// Indirect name maps are a vector of indirect name associations, which consist of
	// an index and a name map forming pairs. This creates a mapping of index -> index -> name
	// as two map levels are formed. Indices have to appear in order.
	// https://webassembly.github.io/spec/core/appendix/custom.html#name-maps

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
	// A function type is expected to start with the byte 0x60 followed
	// by two result types, which are vectors of valtypes.
	// https://webassembly.github.io/spec/core/binary/types.html#function-types

	assertU8(0x60);

	// Just append to the vector to keep the parameters
	auto numParameters = parseResultTypeVector().size();
	auto numResults = parseResultTypeVector(false).size()- numParameters;

	auto it= cachedResultTypeVector.begin();
	std::span<ValType> parameterSlice{ it, it + numParameters };
	std::span<ValType> resultSlice{ it + numParameters, it + numParameters + numResults };
	
	return { parameterSlice, resultSlice };
}

std::vector<ValType>& ModuleParser::parseResultTypeVector(bool doClearVector)
{
	// A result type is a vector of valtypes. Instead of allocating a new
	// vector for each call of the method a common vector is recycled and 
	// cleared each time.
	// https://webassembly.github.io/spec/core/binary/types.html#result-types

	if (doClearVector) {
		cachedResultTypeVector.clear();
	}

	auto resultNum = nextU32();
	cachedResultTypeVector.reserve(cachedResultTypeVector.size()+ resultNum);
	for (u32 i = 0; i != resultNum; i++) {
		auto valType = ValType::fromInt(nextU8());
		if (!valType.isValid()) {
			throwParsingError("Found invalid val type while parsing result type vector");
		}
		cachedResultTypeVector.push_back( valType );
	}

	return cachedResultTypeVector;
}

TableType ModuleParser::parseTableType()
{
	// A table type consists of a ref type, which is a subset of
	// valtypes, followed by a limits object.
	// https://webassembly.github.io/spec/core/binary/types.html#table-types

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
	// A memory type consists of a single limits object.
	// https://webassembly.github.io/spec/core/binary/types.html#memory-types

	if (!hasNext()) {
		throwParsingError("Not enough bytes to parse memory type");
	}
	
	return { parseLimits()};
}

GlobalType ModuleParser::parseGlobalType()
{
	// Global types consist of a valtype followed by byte flag indicating
	// whether the global is mutable.
	// https://webassembly.github.io/spec/core/binary/types.html#global-types

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
	// Each global consists of a global type object followed by a constant init 
	// expression.
	// https://webassembly.github.io/spec/core/binary/modules.html#global-section

	auto globalType = parseGlobalType();
	auto initExpressionCode = parseInitExpression();

	return { globalType, initExpressionCode };
}

Limits ModuleParser::parseLimits()
{
	// A limits object starts with a byte flag inicating whether a max value
	// is present. Either only a min value or a min and a max value follow.
	// https://webassembly.github.io/spec/core/binary/types.html#limits

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
	// Init expressions are expressions which may only contain constant 
	// instructions. They are therefore byte sequences of a small set of
	// possible instructions. As loops, blocks and branches are not constant
	// they may not have an nesting, which makes parsing them simpler as the
	// first END marks their end.
	// https://webassembly.github.io/spec/core/binary/instructions.html#expressions
	// https://webassembly.github.io/spec/core/valid/instructions.html#constant-expressions

	// FIXME: All constant instructions have a producing stack effect, which means
	// that only a single instruction could ever be read in. So maybe ditch the vector

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

std::vector<ModuleFunctionIndex> ModuleParser::parseU32Vector()
{
	auto numExp = nextU32();
	std::vector<ModuleFunctionIndex> ints;
	ints.reserve(numExp);

	for (u32 i = 0; i != numExp; i++) {
		ModuleFunctionIndex funcIdx{ nextU32() };
		ints.emplace_back(funcIdx);
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
	ModuleExportIndex index{ nextU32() };

	return { std::move(name), exportType, index };
}

Element ModuleParser::parseElement()
{
	// Each element can either be active, passive or declarative. Elements
	// can have different structures that declare a type, either a vector of
	// function indices or init expressions, and possibly a table index and offset
	// expression. A u32 a the very beginning defines how the data needs to be
	// interpreted. There are currently 8 interpretations recognized.
	// https://webassembly.github.io/spec/core/binary/modules.html#element-section

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
		return { ElementMode::Active, ValType::FuncRef, ModuleTableIndex{ 0 }, tableOffset, std::move(functions) };
	}
	case 1:
	{
		parseElementKind();
		auto functions = parseU32Vector();
		return {ElementMode::Passive, ValType::FuncRef, std::move(functions)};
	}
	case 2:
	{
		ModuleTableIndex tableIdx{ nextU32() };
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
		return {ElementMode::Active, ValType::FuncRef, ModuleTableIndex{ 0 }, tableOffset, std::move(exprs)};
	}
	case 5:
	{
		auto refType = parseReferenceType();
		auto exprs = parseInitExpressionVector();
		return {ElementMode::Passive, refType, std::move(exprs)};
	}
	case 6:
	{
		ModuleTableIndex tableIdx{ nextU32() };
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
	// A function code item starts with its size in bytes, followed by
	// a vector of locals. Each local has number of how many of its type
	// exist and its type. This creates something comparable to run-length
	// encoding for the local types which are decoded into a vector of
	// compressed locals. After this an expression contains the actual
	// body/code of the function.
	// https://webassembly.github.io/spec/core/binary/modules.html#code-section

	auto byteCount = nextU32();
	auto posBeforeLocals = it;
	auto numLocals = nextU32();
	
	// Read the locals
	std::vector<CompressedLocalTypes> locals;
	for (u32 i = 0; i != numLocals; i++) {
		auto localCount = nextU32();
		auto localType = ValType::fromInt(nextU8());
		locals.emplace_back(localCount, localType);
	}

	// Create buffer slice of the function body and convert it
	// to an array of instructions
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

FunctionType::FunctionType(std::span<const ValType> parameters, std::span<const ValType> results)
	: storage{ LocalArray{.numParameters = 0, .numResults = 0} } {

	ValType* arrayPtr;
	auto arrayLength = parameters.size() + results.size();
	if (arrayLength <= LocalArray::maxStoredEntries) {
		// Use the local array
		auto& local = std::get<0>(storage);
		local.numParameters = parameters.size();
		local.numResults = results.size();
		arrayPtr = local.array;
	}
	else {
		// Allocate an array on the heap, because there are too many items to store locally
		storage = HeapArray{
		.array = std::make_unique<ValType[]>(arrayLength),
		.numParameters = parameters.size(),
		.numResults = results.size()
		};

		arrayPtr = std::get<1>(storage).array.get();
		assert(arrayPtr);
	}

	// Copy to array
	for (u32 i = 0; i != parameters.size(); i++) {
		arrayPtr[i] = parameters[i];
	}

	for (u32 i = 0; i != results.size(); i++) {
		arrayPtr[i + parameters.size()] = results[i];
	}
}

FunctionType::FunctionType(const FunctionType& other)
	: FunctionType{ other.parameters(), other.results() } {}

const std::span<const ValType> FunctionType::parameters() const
{
	if (isLocalArray()) {
		return { asLocalArray().array, asLocalArray().numParameters };
	}

	return { asHeapArray().array.get(), asHeapArray().numParameters };
}

const std::span<const ValType> FunctionType::results() const
{
	if (isLocalArray()) {
		return { asLocalArray().array + asLocalArray().numParameters, asLocalArray().numResults };
	}

	return { asHeapArray().array.get() + asHeapArray().numParameters, asHeapArray().numResults };
}

bool FunctionType::returnsVoid() const
{
	return results().empty();
}

bool FunctionType::takesVoidReturnsVoid() const
{
	return parameters().empty() && results().empty();
}

u32 FunctionType::parameterStackSectionSizeInBytes() const
{
	if (requiredParameterStackBytes.has_value()) {
		return *requiredParameterStackBytes;
	}

	u32 numBytesParameters = 0;
	for (auto valType : parameters()) {
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
	for (auto valType : results()) {
		numBytesResults += valType.sizeInBytes();
	}
	requiredResultStackBytes = numBytesResults;

	return numBytesResults;
}

bool WASM::FunctionType::takesValuesAsParameters(std::span<Value> values) const
{
	auto params = parameters();
	if (params.size() != values.size()) {
		return false;
	}

	for (u32 i = 0; i != params.size(); i++) {
		if (params[i] != values[i].type()) {
			return false;
		}
	}

	return true;
}

void FunctionType::print(std::ostream& out) const
{
	out << "Function: ";

	if (isLocalArray()) {
		out << "(local) ";
	}
	else {
		out << "(heap) ";
	}

	for (auto& param : parameters()) {
		out << param.name() << ' ';
	}

	if (parameters().empty()) {
		out << "<none> ";
	}

	out << "-> ";

	for (auto& result : results()) {
		out << result.name() << ' ';
	}

	if (results().empty()) {
		out << "<none>";
	}
}

bool FunctionType::operator==(const FunctionType& other) const
{
	if (this == &other) {
		return true;
	}

	if (parameters().size() != other.parameters().size() || results().size() != other.results().size()) {
		return false;
	}

	for (u32 i = 0; i != parameters().size(); i++) {
		if (parameters()[i] != other.parameters()[i]) {
			return false;
		}
	}

	for (u32 i = 0; i != results().size(); i++) {
		if (results()[i] != other.results()[i]) {
			return false;
		}
	}

	return true;
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
	// The min value must be smaller or equal to the specified range for a 
	// limit to be valid. Further it must be smaller or equal to the max
	// value if one is present. The max value must also be smaller or equal
	// to the specified range, or be not present.
	// https://webassembly.github.io/spec/core/valid/types.html#valid-limits

	if (mMin > range) {
		return false;
	}

	if (mMax.has_value()) {
		return *mMax <= range && mMin <= *mMax;
	}

	return true;
}

bool Limits::matches(const Limits& other) const
{
	// To have this limits object match another limits object the min value
	// has to be greater or equal to the other. If the other does not have 
	// a max value return true. If the other has a max value, this object 
	// needs to have one too, which also has to be smaller or equal.
	// https://webassembly.github.io/spec/core/valid/types.html#match-limits

	if (mMin < other.mMin) {
		return false;
	}

	if (!other.mMax.has_value()) {
		return true;
	}

	return other.mMax.has_value() && mMax.has_value() && *mMax <= *other.mMax;
}

void TableType::print(std::ostream& out) const
{
	out << "Table: " << mElementReferenceType.name() << ' ';
	mLimits.print(out);
}

void DeclaredHostGlobal::setIndexInTypedStorageArray(ModuleGlobalTypedArrayIndex idx)
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
	// Validating an export has to validate the external type it exports. As each declared type in the module
	// is validated separately it only needs to be checked whether the export references a valid type.
	// https://webassembly.github.io/spec/core/valid/modules.html#exports
	// https://webassembly.github.io/spec/core/valid/types.html#external-types

	switch (mExportType) {
	case ExportType::FunctionIndex: return mIndex < numFunctions;
	case ExportType::TableIndex: return mIndex < numTables;
	case ExportType::MemoryIndex: return mIndex < numMemories;
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

LinkedElement Element::decodeAndLink(ModuleElementIndex index, Module& module)
{
	u32 tableOffset = mTablePosition.has_value() ? mTablePosition->tableOffset.constantI32() : 0;
	if (mMode == ElementMode::Passive) {
		return { index, mMode, refType, tableIndex(), tableOffset };
	}

	std::vector<Nullable<Function>> functionPointers;
	auto appendFunction = [&](ModuleFunctionIndex functionIdx) {
		auto function= module.functionByIndex(functionIdx);
		assert(function.has_value()); // FIXME: Throw instead
		functionPointers.emplace_back(function);
	};

	if (mInitExpressions.index() == 1) {
		auto& expressionVector = std::get<1>(mInitExpressions);
		functionPointers.reserve(expressionVector.size());
		for (auto& expr : expressionVector) {
			// Do not allow null references
			// FIXME: Throw instead
			auto funcIndex = expr.constantFuncRefAsIndex();
			assert(funcIndex.has_value());

			appendFunction(*funcIndex);
		}
	}
	else {
		auto& indexVector = std::get<0>(mInitExpressions);
		functionPointers.reserve(indexVector.size());
		for (auto idx : indexVector) {
			appendFunction(idx);
		}
	}

	return {
		index,
		mMode,
		refType,
		tableIndex(),
		(u32)tableOffset,
		std::move(functionPointers)
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

std::optional<ModuleFunctionIndex> Expression::constantFuncRefAsIndex() const
{
	assert(mInstructions.size() > 0);
	return mInstructions.front().asReferenceIndex();
}

u64 Expression::constantUntypedValue(Module& module) const
{
	assert(mInstructions.size() > 0);

	auto& instruction = mInstructions.front();
	assert(instruction.isConstant());

	switch (instruction.opCode()) {
	case InstructionType::I32Const:
	case InstructionType::F32Const:
		return instruction.asIF32Constant();
	case InstructionType::I64Const:
	case InstructionType::F64Const:
		return instruction.asIF64Constant();
	case InstructionType::ReferenceNull:
		return 0;
	case InstructionType::ReferenceFunction: {
		auto function= module.functionByIndex(instruction.functionIndex());
		assert(function.has_value());
		return (u64)function.pointer();
	}
	case InstructionType::GlobalGet:
		assert(false); // Not yet supported
	}
	return 0;
}

void ModuleValidator::validate(const ParsingState& parser)
{
	// Validate the parsed module state by validating all of its parts
	// under the context C and the reduced context C'
	// https://webassembly.github.io/spec/core/valid/modules.html#valid-module

	parsingState = &parser;

	if (introspector.has_value()) {
		introspector->onModuleValidationStart();
	}

	// Under module context C

	if (s().functions.size() != s().functionCodes.size()) {
		throwValidationError("Parsed different number of function declarations than function codes");
	}

	if (s().memoryTypes.size() + s().importedMemoryTypes.size() > 1) {
		throwValidationError("Cannot define or import more than one memory");
	}

	for (u32 i = 0; i != s().functions.size(); i++) {
		validateFunction(LocalFunctionIndex{ i });
	}

	if (s().startFunctionIndex.has_value()) {
		validateStartFunction(*s().startFunctionIndex);
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

	if (introspector.has_value()) {
		introspector->onModuleValidationFinished();
	}

	parsingState = nullptr;
}

const FunctionType& ModuleValidator::functionTypeByIndex(ModuleFunctionIndex funcIdx)
{
	ModuleTypeIndex typeIdx{ 0 };
	if (funcIdx < s().importedFunctions.size()) {
		typeIdx= s().importedFunctions[funcIdx.value].moduleTypeIndex();
	}
	else {
		funcIdx -= s().importedFunctions.size();
		if (funcIdx > s().functions.size()) {
			throwValidationError("Invalid function index");
		}

		typeIdx = s().functions[funcIdx.value];
	}
	
	if (typeIdx > s().functionTypes.size()) {
		throwValidationError("Function references invalid type index");
	}
	return s().functionTypes[typeIdx.value];
}

void ModuleValidator::validateFunction(LocalFunctionIndex funcNum)
{
	// Validating a function checks whether it references a valid
	// function type. Further its expression has to be checked, however
	// this is done by the compiler. Function types are always valid.
	// https://webassembly.github.io/spec/core/valid/modules.html#functions
	// https://webassembly.github.io/spec/core/valid/types.html#function-types

	auto typeIdx = s().functions[funcNum.value];
	if (typeIdx > s().functionTypes.size()) {
		throwValidationError("Function references invalid type index");
	}
	
	// Validation of the actual function code happens in the compiler

	auto& type = s().functionTypes[typeIdx.value];
	if (introspector.has_value()) {
		ModuleFunctionIndex funcIdx{ (u32)(funcNum.value + s().importedFunctions.size()) };
		introspector->onValidatingFunction(funcIdx, type);
	}
}

void ModuleValidator::validateTableType(const TableType& tableType)
{
	// Validating a table (type) checks whether the limit is valid within the range
	// 0...2^32-1
	// https://webassembly.github.io/spec/core/valid/modules.html#tables
	// https://webassembly.github.io/spec/core/valid/types.html#table-types

	constexpr u32 tableRange = 0xFFFFFFFF;
	if (!tableType.limits().isValid(tableRange)) {
		throwValidationError("Invalid table limits definition");
	}

	if (introspector.has_value()) {
		introspector->onValidatingTableType(tableType);
	}
}

void ModuleValidator::validateMemoryType(const MemoryType& memoryType)
{
	// Validating a memory (type) checks whether the limit is valid within the range
	// 0...2^16
	// https://webassembly.github.io/spec/core/valid/modules.html#memories
	// https://webassembly.github.io/spec/core/valid/types.html#memory-types

	constexpr u32 memoryRange = 0x10000;
	if (!memoryType.limits().isValid(memoryRange)) {
		throwValidationError("Invalid range limits definition");
	}

	if (introspector.has_value()) {
		introspector->onValidatingMemoryType(memoryType);
	}
}

void ModuleValidator::validateExport(const Export& exportItem)
{
	// To validate an export, the exported item has to be validated. The name of
	// the export also has to be unique.
	// https://webassembly.github.io/spec/core/valid/modules.html#exports
	// https://webassembly.github.io/spec/core/syntax/modules.html#exports

	auto numFunctions = s().functions.size() + s().importedFunctions.size();
	auto numTables = s().tableTypes.size() + s().importedTableTypes.size();
	auto numMemories = s().memoryTypes.size() + s().importedMemoryTypes.size();
	auto numGlobals = s().globals.size() + s().importedGlobalTypes.size();
	if (!exportItem.isValid(numFunctions, numTables, numMemories, numGlobals)) {
		throwValidationError("Export references invalid index");
	}

	// Try insert the name and throw if it already exists
	auto result= exportNames.emplace(exportItem.name());
	if ( !result.second ) {
		throwValidationError("Duplicate export name");
	}

	if (introspector.has_value()) {
		introspector->onValidatingExport(exportItem);
	}
}

void ModuleValidator::validateStartFunction(ModuleFunctionIndex idx)
{
	// To validate the start function it has to be checked whether the function index references
	// a valid function. Further the function type has to be [] -> [] (no parameters, no return
	// value).
	// https://webassembly.github.io/spec/core/valid/modules.html#start-function

	auto& funcType = functionTypeByIndex(idx);
	if (!funcType.takesVoidReturnsVoid()) {
		throwValidationError("Start function has wrong type");
	}

	if (introspector.has_value()) {
		introspector->onValidatingStartFunction(idx);
	}
}

void ModuleValidator::validateGlobal(const DeclaredGlobal& global)
{
	// For a global to be valid, its type has to be valid. The init expression has to be 
	// constant, valid and has to result in a type compatible with the global's type.
	// https://webassembly.github.io/spec/core/valid/modules.html#globals

	validateConstantExpression(global.initExpression(), global.valType());

	if (introspector.has_value()) {
		introspector->onValidatingGlobal(global);
	}
}

void ModuleValidator::validateElementSegment(const Element& elem)
{
	// For a element segement to be valid, each init expression has to be constant, valid and
	// result in a type compatible with the table type. Further, the element mode has to be
	// valid:
	// - Passive mode: Always valid.
	// - Active mode: The element has to reference a valid table. The element's table offset expression
	//   has to be constant, valid and result in a I32.
	// - Declarative mode: Always valid.
	// https://webassembly.github.io/spec/core/valid/modules.html#element-segments

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

		auto& table = s().tableTypes[tableIdx.value];
		if (table.valType() != elem.valType()) {
			throwValidationError("Element segment type missmatch with reference table");
		}

		auto& tablePos = elem.tablePosition();
		assert(tablePos.has_value());
		
		validateConstantExpression(tablePos->tableOffset, ValType::I32);
	}

	if (introspector.has_value()) {
		introspector->onValidatingElement(elem);
	}
}

void ModuleValidator::validateImports()
{
	// Each import is validated based on the kind of imported item.
	// - Function: The import has to reference a valid function type by index.
	// - Table: The table type has to be valid.
	// - Memory: The memory type has to be valid.
	// - Global: The global type has to be valid.
	// https://webassembly.github.io/spec/core/valid/modules.html#imports

	if (introspector.has_value()) {
		introspector->onModulImportsValidationStart();
	}

	// TODO: Validate function imports
	
	// Validate table imports
	for (auto& tableType : s().importedTableTypes) {
		validateTableType(tableType.tableType());
	}
	
	// Validate memory imports
	for (auto& memType : s().importedMemoryTypes) {
		validateMemoryType(memType.memoryType());
	}
	
	// Validate global imports -> global types are valid

	if (introspector.has_value()) {
		introspector->onModulImportsValidationFinished();
	}
}

void ModuleValidator::validateConstantExpression(const Expression& exp, ValType expectedType)
{
	// For an expression to be constant and valid, all of its instruction have to be constant.
	// The last instruction has to be an 'End' instruction. The instructions may have a stack
	// effect. At the end of the expression only one value may be on the stack, which has to 
	// have a type compatible with the specified expected result type.
	// https://webassembly.github.io/spec/core/valid/instructions.html#constant-expressions

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
	startFunctionIndex.reset();
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
	if (mGlobalType.valType().sizeInBytes() == 4) {
		auto& ptr= *mResolvedGlobal32;
		return ptr;
	}

	auto& ptr = *mResolvedGlobal64;
	return ptr;
}

ExportType GlobalImport::requiredExportType() const
{
	return ExportType::GlobalIndex;
}

bool GlobalImport::isResolved() const
{
	if (mGlobalType.valType().sizeInBytes() == 4) {
		return mResolvedGlobal32.has_value();
	}

	return mResolvedGlobal64.has_value();
}

bool WASM::GlobalImport::resolveFromResolvedGlobal(std::optional<ResolvedGlobal> resolvedGlobal)
{
	if (!resolvedGlobal.has_value()) {
		return false;
	}

	// Check if types match. Return true even if the types do not match, because
	// the type was still found altough it could not be matched. 'isTypeCompatible()'
	// therefore only checks if something was resolved as the type checking happens here
	// already.
	// Types are compatible if they are the same.
	// https://webassembly.github.io/spec/core/valid/types.html#globals
	auto expectedBytes = mGlobalType.valType().sizeInBytes();
	if (expectedBytes != resolvedGlobal->type.valType().sizeInBytes()) {
		if (expectedBytes == 4) {
			mResolvedGlobal32.clear();
		}
		else {
			mResolvedGlobal64.clear();
		}

		return true;
	}

	// TODO: Remove the ugly const casts
	if (expectedBytes == 4) {
		mResolvedGlobal32 = const_cast<Global<u32>&>(reinterpret_cast<const Global<u32>&>(resolvedGlobal->instance));
	}
	else {
		mResolvedGlobal64 = const_cast<Global<u64>&>(reinterpret_cast<const Global<u64>&>(resolvedGlobal->instance));
	}

	return true;
}

bool GlobalImport::tryResolveFromModuleWithIndex(Module& module, ModuleExportIndex idx)
{
	ModuleGlobalIndex globalIdx{ idx.value };
	return resolveFromResolvedGlobal(module.globalByIndex(globalIdx));	
}

bool GlobalImport::tryResolveFromModuleWithName(ModuleBase& module)
{
	return resolveFromResolvedGlobal(module.exportedGlobalByName(mName));
}

bool GlobalImport::isTypeCompatible() const
{
	// Type checking happens in 'tryResolvefromModuleWithIndex()'
	return isResolved();
}

void FunctionImport::interpreterTypeIndex(InterpreterTypeIndex idx)
{
	if (hasInterpreterTypeIndex()) {
		throw std::runtime_error{"Function import already has a deduplicated function type index"};
	}

	mInterpreterTypeIndex = idx;
}

ExportType FunctionImport::requiredExportType() const
{
	return ExportType::FunctionIndex;
}

bool FunctionImport::isResolved() const
{
	return mResolvedFunction.has_value();
}

bool FunctionImport::tryResolveFromModuleWithIndex(Module& module, ModuleExportIndex idx)
{
	ModuleFunctionIndex funcIdx{ idx.value };
	mResolvedFunction = module.functionByIndex(funcIdx);
	return isResolved();
}

bool FunctionImport::tryResolveFromModuleWithName(ModuleBase& module)
{
	mResolvedFunction = module.exportedFunctionByName(mName);
	return isResolved();
}

bool FunctionImport::isTypeCompatible() const
{
	// Types are compatible if they are the same.
	// https://webassembly.github.io/spec/core/valid/types.html#functions

	return isResolved()
		&& mResolvedFunction->interpreterTypeIndex() == *mInterpreterTypeIndex;
}

ExportType MemoryImport::requiredExportType() const
{
	return ExportType::MemoryIndex;
}

bool MemoryImport::isResolved() const
{
	return mResolvedMemory.has_value();
}

bool MemoryImport::tryResolveFromModuleWithIndex(Module& module, ModuleExportIndex idx)
{
	ModuleMemoryIndex memIdx{ idx.value };
	mResolvedMemory = module.memoryByIndex(memIdx);
	return isResolved();
}

bool MemoryImport::tryResolveFromModuleWithName(ModuleBase& module)
{
	mResolvedMemory = module.exportedMemoryByName(mName);
	return isResolved();
}

bool MemoryImport::isTypeCompatible() const
{
	// Types are compatible if the external memory limits match the import declaration.
	// https://webassembly.github.io/spec/core/valid/types.html#memories

	return isResolved()
		&& mResolvedMemory->limits().matches(mMemoryType.limits());
}

ExportType TableImport::requiredExportType() const
{
	return ExportType::TableIndex;
}

bool TableImport::isResolved() const
{
	return mResolvedTable.has_value();
}

bool TableImport::tryResolveFromModuleWithIndex(Module& module, ModuleExportIndex idx)
{
	ModuleTableIndex tableIdx{ idx.value };
	mResolvedTable = module.tableByIndex(tableIdx);
	return isResolved();
}

bool TableImport::tryResolveFromModuleWithName(ModuleBase& module)
{
	mResolvedTable = module.exportedTableByName(mName);
	return isResolved();
}

bool TableImport::isTypeCompatible() const
{
	// Types are compatible if the external table limits match the import declaration, 
	// and if both val types are the same.
	// https://webassembly.github.io/spec/core/valid/types.html#tables

	return isResolved()
		&& mResolvedTable->type() == mTableType.valType()
		&& mResolvedTable->limits().matches(mTableType.limits());
}

std::string Imported::scopedName() const
{
	std::string itemName;
	itemName.reserve(mModule.size() + mName.size() + 2);
	itemName += mModule;
	itemName += "::";
	itemName += mName;
	return itemName;
}

ModuleFunctionIndex WASM::ExportItem::asFunctionIndex() const
{
	assert(mExportType == ExportType::FunctionIndex);
	return ModuleFunctionIndex{ mIndex.value };
}

ModuleGlobalIndex WASM::ExportItem::asGlobalIndex() const
{
	assert(mExportType == ExportType::GlobalIndex);
	return ModuleGlobalIndex{ mIndex.value };
}

ModuleMemoryIndex WASM::ExportItem::asMemoryIndex() const
{
	assert(mExportType == ExportType::MemoryIndex);
	return ModuleMemoryIndex{ mIndex.value };
}

ModuleTableIndex WASM::ExportItem::asTableIndex() const
{
	assert(mExportType == ExportType::TableIndex);
	return ModuleTableIndex{ mIndex.value };
}
