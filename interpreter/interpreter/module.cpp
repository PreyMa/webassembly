
#include <cassert>
#include <iomanip>
#include <iostream>

#include "interpreter.h"
#include "error.h"

using namespace WASM;

static constexpr inline bool isShortDistance(i32 distance) {
	return distance >= -128 && distance <= 127;
}

BytecodeFunction::BytecodeFunction(u32 idx, u32 ti, FunctionType& t, FunctionCode&& c)
	: mIndex{ idx }, mTypeIndex{ ti }, type{ t }, code{ std::move(c.code) } {
	uncompressLocalTypes(c.compressedLocalTypes);
}

std::optional<BytecodeFunction::LocalOffset> BytecodeFunction::localByIndex(u32 idx) const
{
	if (idx < uncompressedLocals.size()) {
		return uncompressedLocals[idx];
	}

	return {};
}

bool BytecodeFunction::hasLocals() const
{
	return type.parameters().size() < uncompressedLocals.size();
}

u32 BytecodeFunction::operandStackSectionOffsetInBytes() const
{
	if (uncompressedLocals.empty()) {
		return 0;
	}

	auto& lastLocal = uncompressedLocals.back();
	auto byteOffset= lastLocal.offset + lastLocal.type.sizeInBytes();

	// Manually add the size of FP + SP+ MP, if there are only parameters
	if (!hasLocals()) {
		byteOffset += 24;
	}

	return byteOffset;
}

u32 BytecodeFunction::localsSizeInBytes() const
{
	if (!hasLocals()) {
		return 0;
	}

	u32 beginLocalsByteOffset = uncompressedLocals[type.parameters().size()].offset;
	u32 endLocalsByteOffset = operandStackSectionOffsetInBytes();

	return endLocalsByteOffset - beginLocalsByteOffset;
}

bool WASM::BytecodeFunction::requiresModuleInstance() const
{
	for (auto& ins : code) {
		if (ins.opCode().requiresModuleInstance()) {
			return true;
		}
	}

	return false;
}

Nullable<const std::string> WASM::BytecodeFunction::lookupName(const Module& module)
{
	return module.functionNameByIndex(mIndex);
}

void WASM::BytecodeFunction::uncompressLocalTypes(const std::vector<CompressedLocalTypes>& compressedLocals)
{
	// Count the parameters and locals
	auto& params = type.parameters();
	u32 numLocals = params.size();
	for (auto& pack : compressedLocals) {
		numLocals += pack.count;
	}

	uncompressedLocals.reserve(numLocals);

	// Put all parameters
	u32 byteOffset = 0;
	for (auto param : params) {
		uncompressedLocals.emplace_back(param, byteOffset);
		byteOffset += param.sizeInBytes();
	}

	// Leave space for stack, frame and module pointer
	byteOffset += 24;

	// Decompress and put each local
	for (auto& pack : compressedLocals) {
		for (u32 i = 0; i != pack.count; i++) {
			uncompressedLocals.emplace_back(pack.type, byteOffset);
			byteOffset += pack.type.sizeInBytes();
		}
	}
}

FunctionTable::FunctionTable(u32 idx, const TableType& tableType)
	: index{ idx }, type{ tableType.valType() }, limits{ tableType.limits() }
{
	if (grow(limits.min(), {}) != 0) {
		throw std::runtime_error{ "Could not init table" };
	}
}

i32 FunctionTable::grow(i32 increase, Nullable<Function> item)
{
	auto oldSize = table.size();
	if (limits.max().has_value() && oldSize + increase > *limits.max()) {
		return -1;
	}

	try {
		table.reserve(oldSize + increase);
		table.insert(table.end(), increase, item);
		return oldSize;
	}
	catch (std::bad_alloc& e) {
		return -1;
	}
}

void FunctionTable::init(const DecodedElement& element, u32 tableOffset, u32 elementOffset)
{
	// This has to be done during the linking step
	assert(false);
}


void DecodedElement::initTableIfActive(std::vector<FunctionTable>& tables)
{
	if (mMode != ElementMode::Active) {
		return;
	}

	assert(tableIndex < tables.size());
	tables[tableIndex].init(*this, tableOffset, 0);
}

Memory::Memory(u32 idx, Limits l)
	: index{ idx }, limits{ l } {
	grow(limits.min());
}

i32 Memory::grow(i32 pageCountIncrease)
{
	auto oldByteSize = data.size();
	auto oldPageCount = oldByteSize / PageSize;

	if (limits.max().has_value() && oldPageCount + pageCountIncrease > *limits.max()) {
		return -1;
	}

	try {
		auto byteSizeIncrease = pageCountIncrease * PageSize;
		data.reserve(oldByteSize + byteSizeIncrease);
		data.insert(data.end(), byteSizeIncrease, 0x00);
		return oldPageCount;
	}
	catch (std::bad_alloc& e) {
		return -1;
	}
}

u64 Memory::minBytes() const {
	return limits.min() * PageSize;
}

std::optional<u64> Memory::maxBytes() const {
	auto m = limits.max();
	if (m.has_value()) {
		return { *m * PageSize };
	}

	return {};
}

Module::Module(
	Buffer b,
	std::string p,
	std::string n,
	std::vector<FunctionType> ft,
	std::vector<BytecodeFunction> fs,
	std::vector<FunctionTable> ts,
	std::optional<Memory> ms,
	ExportTable ex,
	std::vector<DeclaredGlobal> gt,
	std::vector<Global<u32>> g32,
	std::vector<Global<u64>> g64,
	std::vector<FunctionImport> imFs,
	std::vector<TableImport> imTs,
	std::optional<MemoryImport> imMs,
	std::vector<GlobalImport> imGs,
	ParsingState::NameMap fns
)
	: path{ std::move(p) },
	mName{ std::move(n) },
	data{ std::move(b) },
	functionTypes{ std::move(ft) },
	functions{ std::move(fs) },
	functionTables{ std::move(ts) },
	globals32{ std::move(g32) },
	globals64{ std::move(g64) },
	ownedMemoryInstance{ std::move(ms) },
	compilationData{
		std::make_unique<Module::CompilationData>(
			std::move(imFs),
			std::move(imTs),
			std::move(imMs),
			std::move(imGs),
			std::move(gt)
		)
	},
	exports{ std::move(ex) },
	functionNameMap{ std::move(fns) }
{
	assert(compilationData);
	numImportedFunctions = compilationData->importedFunctions.size();
	numImportedTables = compilationData->importedTables.size();
	numImportedMemories = compilationData->importedMemory.has_value() ? 1 : 0;
	numImportedGlobals = compilationData->importedGlobals.size();
}


Nullable<Function> Module::functionByIndex(u32 idx)
{
	if (idx < numImportedFunctions) {
		if (compilationData) {
			return compilationData->importedFunctions[idx].resolvedFunction;
		}

		return {};
	}

	idx -= numImportedFunctions;
	assert(idx < functions.size());
	return functions[idx];
}

std::optional<Module::ResolvedGlobal> Module::globalByIndex(u32 idx)
{
	if (!compilationData) {
		return {};
	}

	if (idx < numImportedGlobals) {
		auto& importedGlobal = compilationData->importedGlobals[idx];
		auto baseGlobal = importedGlobal.getBase();
		if (!baseGlobal.has_value()) {
			return {};
		}

		return ResolvedGlobal{
			*baseGlobal,
			importedGlobal.globalType
		};
	}

	idx -= numImportedGlobals;
	assert(idx < compilationData->globalTypes.size());
	auto& declaredGlobal = compilationData->globalTypes[idx];

	assert(declaredGlobal.indexInTypedStorageArray().has_value());
	u32 storageIndex = *declaredGlobal.indexInTypedStorageArray();

	auto& globalType = declaredGlobal.type();
	if (globalType.valType().sizeInBytes() == 4) {
		assert(storageIndex < globals32.size());
		return ResolvedGlobal{ globals32[storageIndex], globalType };
	}

	assert(storageIndex < globals64.size());
	return ResolvedGlobal{ globals64[storageIndex], globalType };
}

Nullable<Memory> WASM::Module::memoryByIndex(u32 idx)
{
	if (idx != 0) {
		return {};
	}

	if (numImportedMemories) {
		if (compilationData) {
			assert(compilationData->importedMemory.has_value());
			return compilationData->importedMemory->resolvedMemory;
		}

		return {};
	}

	assert(ownedMemoryInstance.has_value());
	return *ownedMemoryInstance;
}

std::optional<ExportItem> WASM::Module::exportByName(const std::string& name, ExportType type) const
{
	auto findFunction = exports.find(name);
	if (findFunction == exports.end()) {
		return {};
	}

	auto& exp = findFunction->second;
	if (exp.mExportType != type) {
		return {};
	}

	return exp;
}

Nullable<Function> Module::exportedFunctionByName(const std::string& name)
{
	auto exp = exportByName(name, ExportType::FunctionIndex);
	if (!exp.has_value()) {
		return {};
	}

	return functionByIndex(exp->mIndex);
}

Nullable<const std::string> WASM::Module::functionNameByIndex(u32 functionIdx) const
{
	auto fnd = functionNameMap.find(functionIdx);
	if (fnd == functionNameMap.end()) {
		return {};
	}

	return fnd->second;
}

void ModuleLinker::link()
{
	//if (!module.needsLinking()) {
//		throwCompilationError("Module already linked");
//	}

	// TODO: Linking
	// for (auto& func : module.compilationData->importedFunctions) {
		// func.
	// }

	// TODO: Do linking here

	// TODO: Init globals here

	// FIXME: This is some hard coded linking just for testing
	assert(modules.size() == 1);
	assert(modules[0].compilationData);
	assert(modules[0].compilationData->importedFunctions.size() == 1);

	static HostFunction abortFunction = [&](u32, u32, u32, u32) { std::cout << "Abort called"; };
	std::cout << "Registered function: ";
	abortFunction.print(std::cout);
	std::cout << std::endl;

	modules[0].compilationData->importedFunctions[0].resolvedFunction = abortFunction;
}



std::optional<sizeType> ModuleCompiler::LabelTypes::size(const Module& module) const
{
	u32 typeIndex= 0;
	if (isParameters()) {
		if (!asParameters().has_value()) {
			return 0;
		}

		typeIndex = *asParameters();
	}
	else {
		if (asResults().blockType == BlockType::None) {
			return 0;
		}

		if (asResults().blockType == BlockType::ValType) {
			return 1;
		}

		typeIndex = asResults().index;
	}

	if (typeIndex >= module.functionTypes.size()) {
		return {};
	}

	auto& functionType = module.functionTypes[typeIndex];
	if (isParameters()) {
		return functionType.parameters().size();
	}

	return functionType.results().size();
}

ModuleCompiler::LabelTypes ModuleCompiler::ControlFrame::labelTypes() const
{
	if (opCode == InstructionType::Loop) {
		return { blockTypeIndex.parameters() };
	}

	return { blockTypeIndex.results() };
}

void ModuleCompiler::ControlFrame::appendAddressPatchRequest(ModuleCompiler& comp, AddressPatchRequest request)
{
	if (addressPatchList.has_value()) {
		addressPatchList = comp.addressPatches.add(*addressPatchList, request);
	}
	else {
		addressPatchList = comp.addressPatches.add(request);
	}
}

void ModuleCompiler::ControlFrame::processAddressPatchRequests(ModuleCompiler& comp)
{
	// Loops do not need any patching, as they only receive back jumps
	if (opCode == InstructionType::Loop) {
		return;
	}

	// Patch the jump printed by the if-bytecode, if there was no else-block
	if (elseLabelAddressPatch.has_value()) {
		comp.patchAddress(*elseLabelAddressPatch);
	}

	while (addressPatchList.has_value()) {
		auto& request = comp.addressPatches[*addressPatchList];
		comp.patchAddress(request);

		addressPatchList = comp.addressPatches.remove(*addressPatchList);
	}
}

void ModuleCompiler::compile()
{
	for (auto& function : module.functions) {
		compileFunction(function);
	}

	// Clear the imports
	module.compilationData.reset();
}

void ModuleCompiler::setFunctionContext(const BytecodeFunction& function)
{
	currentFunction = &function;
}

void ModuleCompiler::compileFunction(BytecodeFunction& function)
{
	resetBytecodePrinter();

	setFunctionContext(function);

	auto typeIdx = function.typeIndex();
	controlStack.emplace_back(InstructionType::NoOperation, BlockTypeIndex{ BlockType::TypeIndex, typeIdx }, 0, 0, false, 0);

	// Print entry bytecode if the function has any locals or requires the module instance
	auto localsSizeInBytes = function.localsSizeInBytes();
	if (localsSizeInBytes > 0 || function.requiresModuleInstance()) {
		assert(localsSizeInBytes % 4 == 0);
		print(Bytecode::Entry);
		printPointer(&module);
		printU32(localsSizeInBytes / 4);
	}

	u32 insCounter = 0;
	for (auto& ins : function.expression()) {
		compileInstruction(ins, insCounter++);
	}

	assert(maxStackHeightInBytes % 4 == 0);
	function.setMaxStackHeight(maxStackHeightInBytes / 4);
	
	auto modName = module.name();
	if (modName.size() > 20) {
		modName = "..." + modName.substr(modName.size() - 17);
	}

	auto maybeFunctionName = function.lookupName(module);
	auto* functionName = maybeFunctionName.has_value() ? maybeFunctionName->c_str() : "<unknown name>";
	std::cout << "Compiled function " << modName << " :: " << functionName << " (index " << function.index() << ")" << " (max stack height " << maxStackHeightInBytes/4 << " slots)" << std::endl;
	printBytecode(std::cout);
}

void ModuleCompiler::pushValue(ValType type)
{
	valueStack.emplace_back(type);
	stackHeightInBytes += type.sizeInBytes();
	maxStackHeightInBytes = std::max(maxStackHeightInBytes, stackHeightInBytes);
}

void WASM::ModuleCompiler::pushMaybeValue(ValueRecord record)
{
	if (record.has_value()) {
		pushValue(*record);
	}
	else {
		valueStack.emplace_back(record);
		assert(!isReachable());
	}
}

void ModuleCompiler::pushValues(const std::vector<ValType>& types)
{
	valueStack.reserve(valueStack.size() + types.size());
	valueStack.insert(valueStack.end(), types.begin(), types.end());

	for (auto type : types) {
		stackHeightInBytes += type.sizeInBytes();
		maxStackHeightInBytes = std::max(maxStackHeightInBytes, stackHeightInBytes);
	}
}

void ModuleCompiler::pushValues(const std::vector<ValueRecord>& types)
{
	valueStack.reserve(valueStack.size() + types.size());
	valueStack.insert(valueStack.end(), types.begin(), types.end());

	for (auto type : types) {
		if (type.has_value()) {
			stackHeightInBytes += type->sizeInBytes();
			maxStackHeightInBytes = std::max(maxStackHeightInBytes, stackHeightInBytes);
		}
	}
}

void ModuleCompiler::pushValues(const BlockTypeParameters& parameters)
{
	if (parameters.has_value()) {
		if (*parameters >= module.functionTypes.size()) {
			throwCompilationError("Block type index references invalid function type");
		}
		auto& type = module.functionTypes[*parameters];
		pushValues(type.parameters());
	}
}

void ModuleCompiler::pushValues(const BlockTypeResults& results)
{
	if (results == BlockType::TypeIndex) {
		if (results.index >= module.functionTypes.size()) {
			throwCompilationError("Block type index references invalid function type");
		}
		auto& type = module.functionTypes[results.index];
		pushValues(type.results());
		return;
	}

	if (results == BlockType::ValType) {
		auto valType = ValType::fromInt(results.index);
		assert(valType.isValid());
		pushValue(valType);
	}
}

void ModuleCompiler::pushValues(const ModuleCompiler::LabelTypes& types)
{
	if (types.isParameters()) {
		pushValues(types.asParameters());
	}
	else {
		pushValues(types.asResults());
	}
}

void ModuleCompiler::resetCachedReturnList(u32 expectedSize)
{
	cachedReturnList.clear();
	cachedReturnList.reserve(expectedSize);
	cachedReturnList.assign(expectedSize, ValueRecord{});
}

BytecodeFunction::LocalOffset ModuleCompiler::localByIndex(u32 idx) const
{
	assert(currentFunction);

	auto local = currentFunction->localByIndex(idx);
	if (local.has_value()) {
		return *local;
	}

	throwCompilationError("Local index out of bounds");
}

Module::ResolvedGlobal WASM::ModuleCompiler::globalByIndex(u32 idx) const
{
	auto global = module.globalByIndex(idx);
	if (global.has_value()) {
		return *global;
	}

	throwCompilationError("Global index out of bounds");
}

const FunctionType& WASM::ModuleCompiler::blockTypeByIndex(u32 idx)
{
	if (idx >= module.functionTypes.size()) {
		throwCompilationError("Block type index references invalid function type");
	}
	return module.functionTypes[idx];
}

const Memory& ModuleCompiler::memoryByIndex(u32 idx)
{
	auto memory= module.memoryByIndex(idx);
	if (memory.has_value()) {
		return *memory;
	}

	throwCompilationError("Memory index out of bounds");
}

u32 WASM::ModuleCompiler::measureMaxPrintedBlockLength(u32 startInstruction, u32 labelIdx, bool runToElse) const
{
	assert(currentFunction);

	if (labelIdx >= controlStack.size()) {
		throwCompilationError("Control stack underflow when measuring block length");
	}

	assert(!runToElse || labelIdx == 0);

	i32 expectedNestingDepth = -labelIdx;
	i32 relativeNestingDepth = 0;
	u32 distance = 0;
	auto& code = currentFunction->expression();
	for (u32 i = startInstruction + 1; i < code.size(); i++) {
		if (code[i] == InstructionType::Block || code[i] == InstructionType::Loop || code[i] == InstructionType::If) {
			relativeNestingDepth++;
		}
		else if (code[i] == InstructionType::End) {
			if (relativeNestingDepth == expectedNestingDepth) {
				return distance;
			}
			relativeNestingDepth--;
		}
		else if (code[i] == InstructionType::Else) {
			if (relativeNestingDepth == 0 && runToElse) {
				return distance;
			}
		}
		distance += code[i].maxPrintedByteLength(currentFunction->expression().bytes());
	}

	throwCompilationError("Invalid block nesting while measuring block length");
}

void WASM::ModuleCompiler::requestAddressPatch(u32 labelIdx, bool isNearJump, bool elseLabel, std::optional<u32> jumpReferencePosition)
{
	if (labelIdx >= controlStack.size()) {
		throwCompilationError("Control stack underflow when requesting address patch");
	}

	auto printerPos = printedBytecode.size();
	AddressPatchRequest req{ printerPos, jumpReferencePosition.value_or(printerPos), isNearJump };
	auto& frame = controlStack[controlStack.size() - labelIdx - 1];

	// Loops do not need address patching as they are always jumped back to
	assert(frame.opCode != InstructionType::Loop);

	if (elseLabel) {
		frame.elseLabelAddressPatch = req;
	}
	else {
		frame.appendAddressPatchRequest(*this, req);
	}

	// Print placeholder values
	if (isNearJump) {
		printU8(0xFF);
	}
	else {
		printU32(0xFF00FF00);
	}
}

void ModuleCompiler::patchAddress(const AddressPatchRequest& request)
{
	auto targetAddress = printedBytecode.size();
	i32 distance = targetAddress - request.jumpReferencePosition;

	assert(!request.isNearJump || isShortDistance(distance));
	if (isReachable()) {
		if (request.isNearJump) {
			printedBytecode[request.locationToPatch] = distance;
		}
		else {
			printedBytecode.writeLittleEndianU32(request.locationToPatch, distance);
		}
	}
}

ModuleCompiler::ValueRecord ModuleCompiler::popValue()
{
	if (controlStack.empty()) {
		throwCompilationError("Control stack is empty");
	}

	auto& frame = controlStack.back();
	if (valueStack.size() == frame.height && frame.unreachable) {
		return {};
	}

	if (valueStack.size() == frame.height) {
		throwCompilationError("Value stack underflows current block height");
	}

	if (valueStack.empty()) {
		throwCompilationError("Value stack underflow");
	}

	auto valueTop = valueStack.back();
	valueStack.pop_back();

	if (valueTop.has_value()) {
		stackHeightInBytes -= valueTop->sizeInBytes();
	}

	return valueTop;
}

ModuleCompiler::ValueRecord ModuleCompiler::popValue(ValueRecord expected)
{
	auto actual = popValue();
	if (!expected.has_value() || !actual.has_value()) {
		return actual;
	}

	if (*expected == *actual) {
		return actual;
	}

	throwCompilationError("Stack types differ");
}

void ModuleCompiler::popValues(const std::vector<ModuleCompiler::ValueRecord>& expected)
{
	/*resetCachedReturnList(expected.size());

	// Iterate in reverse
	auto insertIt = cachedReturnList.rend();
	for (auto it = expected.rbegin(); it != expected.rend(); it++) {
		*(insertIt++) = popValue(*it);
	}

	return cachedReturnList;*/
	for (auto it = expected.rbegin(); it != expected.rend(); it++) {
		popValue(*it);
	}
}

void ModuleCompiler::popValues(const std::vector<ValType>& expected)
{
	for (auto it = expected.rbegin(); it != expected.rend(); it++) {
		popValue(*it);
	}
}

const std::vector<ModuleCompiler::ValueRecord>& ModuleCompiler::popValuesToList(const std::vector<ValType>& expected)
{
	resetCachedReturnList(expected.size());

	// Iterate in reverse
	auto insertIt = cachedReturnList.rbegin();
	for (auto it = expected.rbegin(); it != expected.rend(); it++) {
		*(insertIt++) = popValue(*it);
	}

	return cachedReturnList;
}

const std::vector<ModuleCompiler::ValueRecord>& ModuleCompiler::popValuesToList(const BlockTypeResults& expected) {
	if (expected == BlockType::TypeIndex) {
		auto& type = blockTypeByIndex(expected.index);
		return popValuesToList(type.results());
	}

	if (expected == BlockType::ValType) {
		resetCachedReturnList(1);
		auto valType = ValType::fromInt(expected.index);
		assert(valType.isValid());
		cachedReturnList[0] = popValue(valType);
		return cachedReturnList;
	}

	resetCachedReturnList(0);
	return cachedReturnList;
}

void ModuleCompiler::popValues(const BlockTypeResults& expected) {
	if (expected == BlockType::TypeIndex) {
		auto& type = blockTypeByIndex(expected.index);
		return popValues(type.results());
	}

	if (expected == BlockType::ValType) {
		auto valType = ValType::fromInt(expected.index);
		assert(valType.isValid());
		popValue(valType);
	}
}

const std::vector<ModuleCompiler::ValueRecord>& ModuleCompiler::popValuesToList(const BlockTypeParameters& expected) {
	if (expected.has_value()) {
		auto& type = blockTypeByIndex(*expected);
		return popValuesToList(type.parameters());
	}

	resetCachedReturnList(0);
	return cachedReturnList;
}

void ModuleCompiler::popValues(const BlockTypeParameters& expected) {
	if (expected.has_value()) {
		auto& type = blockTypeByIndex(*expected);
		popValues(type.parameters());
	}
}

const std::vector<ModuleCompiler::ValueRecord>& ModuleCompiler::popValuesToList(const ModuleCompiler::LabelTypes& types)
{
	if (types.isParameters()) {
		return popValuesToList(types.asParameters());
	}

	return popValuesToList(types.asResults());
}

void ModuleCompiler::popValues(const ModuleCompiler::LabelTypes& types)
{
	if (types.isParameters()) {
		popValues(types.asParameters());
		return;
	}

	popValues(types.asResults());
}

ModuleCompiler::ControlFrame& ModuleCompiler::pushControlFrame(InstructionType opCode, BlockTypeIndex blockTypeIndex)
{
	auto& newFrame = controlStack.emplace_back(opCode, blockTypeIndex, valueStack.size(), stackHeightInBytes, false, printedBytecode.size());
	pushValues(blockTypeIndex.parameters());

	return newFrame;
}

ModuleCompiler::ControlFrame ModuleCompiler::popControlFrame()
{
	if (controlStack.empty()) {
		throwCompilationError("Control stack underflow");
	}

	auto& frame = controlStack.back();
	popValues(frame.blockTypeIndex.results());
	if (valueStack.size() != frame.height) {
		throwCompilationError("Value stack height missmatch");
	}

	controlStack.pop_back();
	return frame;
}

void ModuleCompiler::setUnreachable()
{
	if (controlStack.empty()) {
		throwCompilationError("Control stack underflow");
	}

	auto& frame = controlStack.back();
	valueStack.resize(frame.height);
	stackHeightInBytes = frame.heightInBytes;
	frame.unreachable = true;
}

bool WASM::ModuleCompiler::isReachable() const
{
	if (controlStack.empty()) {
		throwCompilationError("Control stack is empty");
	}

	auto& frame = controlStack.back();
	return !frame.unreachable;
}

void ModuleCompiler::resetBytecodePrinter()
{
	printedBytecode.clear();
	valueStack.clear();
	controlStack.clear();
	addressPatches.clear();
	stackHeightInBytes = 0;
	maxStackHeightInBytes = 0;
}

void ModuleCompiler::print(Bytecode c)
{
	std::cout << "  Printed at " << printedBytecode.size() << " bytecode: " << c.name() << std::endl;
	printedBytecode.appendU8(c);
}

void ModuleCompiler::printU8(u8 x)
{
	std::cout << "  Printed at " << printedBytecode.size() << " u8: " << (int) x << std::endl;
	printedBytecode.appendU8(x);
}

void ModuleCompiler::printU32(u32 x)
{
	std::cout << "  Printed at " << printedBytecode.size() << " u32: " << x << std::endl;
	printedBytecode.appendLittleEndianU32(x);
}

void ModuleCompiler::printU64(u64 x)
{
	std::cout << "  Printed at " << printedBytecode.size() << " u64: " << x << std::endl;
	printedBytecode.appendLittleEndianU64(x);
}

void ModuleCompiler::printF32(f32 f)
{
	std::cout << "  Printed at " << printedBytecode.size() << " f32: " << f << " as " << reinterpret_cast<u32&>(f) << std::endl;
	printedBytecode.appendLittleEndianU32(reinterpret_cast<u32&>(f));
}

void ModuleCompiler::printF64(f64 f)
{
	std::cout << "  Printed at " << printedBytecode.size() << " f64: " << f << " as " << reinterpret_cast<u64&>(f) << std::endl;
	printedBytecode.appendLittleEndianU32(reinterpret_cast<u64&>(f));
}

void ModuleCompiler::printPointer(const void* p)
{
	printedBytecode.appendLittleEndianU64(reinterpret_cast<u64&>(p));
	std::cout << "  Printed pointer: " << reinterpret_cast<u64&>(p) << std::endl;
}

void ModuleCompiler::printBytecodeExpectingNoArgumentsIfReachable(Instruction instruction)
{
	if (isReachable() && !instruction.opCode().isBitCastConversionOnly()) {
		auto bytecode = instruction.toBytecode();
		assert(bytecode.has_value());
		print(*bytecode);

		if (bytecode->arguments() != BytecodeArguments::None) {
			throwCompilationError("Bytecode requires unexpected arguments");
		}
	}
}

void WASM::ModuleCompiler::printLocalGetSetTeeBytecodeIfReachable(
	BytecodeFunction::LocalOffset local,
	Bytecode near32,
	Bytecode near64,
	Bytecode far32,
	Bytecode far64
)
{
	if (!isReachable()) {
		return;
	}

	// Check alignment
	assert(local.offset % 4 == 0);
	assert(stackHeightInBytes % 4 == 0);

	u32 operandOffsetInBytes = currentFunction->operandStackSectionOffsetInBytes();
	assert(operandOffsetInBytes % 4 == 0);

	// Full stack size = current operand stack + function parameter section + FP + SP + function locals
	u32 fullStackHeightInSlots = (stackHeightInBytes / 4) + (operandOffsetInBytes / 4);
	u32 localSlotOffset= local.offset / 4;
	u32 distance = fullStackHeightInSlots - localSlotOffset;

	if (local.type.sizeInBytes() == 4) {
		if (distance <= 255) {
			print(near32);
			printU8(distance);
		}
		else {
			print(far32);
			printU32(distance);
		}
	}
	else if (local.type.sizeInBytes() == 8) {
		if (distance) {
			print(near64);
			printU8(distance);
		}
		else {
			print(far64);
			printU32(distance);
		}
	}
	else {
		throwCompilationError("LocalGet instruction only implemented for 32bit and 64bit");
	}
}

void ModuleCompiler::compileNumericConstantInstruction(Instruction instruction)
{
	auto opCode = instruction.opCode();
	auto resultType = opCode.resultType();
	assert(resultType.has_value());
	pushValue(*resultType);

	if (isReachable()) {
		auto bytecode = instruction.toBytecode();
		assert(bytecode.has_value());
		print(*bytecode);

		auto args = bytecode->arguments();
		if (args == BytecodeArguments::SingleU32) {
			printU32(instruction.asIF32Constant());
		}
		else if (args == BytecodeArguments::SingleU64) {
			printU64(instruction.asIF64Constant());
		}
		else {
			throwCompilationError("Bytecode requires unexpected arguments");
		}
	}
}

void ModuleCompiler::compileNumericUnaryInstruction(Instruction instruction)
{
	auto opCode = instruction.opCode();
	auto operandType = opCode.operandType();
	auto resultType = opCode.resultType();
	assert(operandType.has_value() && resultType.has_value());
	popValue(*operandType);
	pushValue(*resultType);

	printBytecodeExpectingNoArgumentsIfReachable(instruction);
}

void ModuleCompiler::compileNumericBinaryInstruction(Instruction instruction) {
	auto opCode = instruction.opCode();
	auto operandType = opCode.operandType();
	auto resultType = opCode.resultType();
	assert(operandType.has_value() && resultType.has_value());
	popValue(*operandType);
	popValue(*operandType);
	pushValue(*resultType);

	printBytecodeExpectingNoArgumentsIfReachable(instruction);
}

void ModuleCompiler::compileMemoryDataInstruction(Instruction instruction)
{
	auto opCode = instruction.opCode();
	auto operandType = opCode.operandType();
	auto resultType = opCode.resultType();

	// Load type instruction
	using IT = InstructionType;
	auto isLoadInstruction = resultType.has_value();
	if (isLoadInstruction) {
		popValue(ValType::I32);
		pushValue(*resultType);
	}
	// Store type instruction
	else {
		assert(operandType.has_value());
		popValue(*operandType);
		popValue(ValType::I32);
	}

	// Print simple bytecode
	auto bytecode = instruction.toBytecode();
	if (bytecode.has_value()) {
		print(*bytecode);
		printU32(instruction.memoryOffset());	
	}

	auto printNearOrFar = [&](Bytecode near, Bytecode far) {
		auto offset = instruction.memoryOffset();
		if (offset <= 255) {
			print(near);
			printU8(offset);
			return;
		}

		print(far);
		printU32(offset);
	};

	// Print bytecode as either near or short instruction
	switch (opCode) {
	case IT::I32Load:
	case IT::F32Load:
		printNearOrFar(Bytecode::I32LoadNear, Bytecode::I32LoadFar);
		break;
	case IT::I64Load:
	case IT::F64Load:
		printNearOrFar(Bytecode::I64LoadNear, Bytecode::I64LoadFar);
		break;
	case IT::I32Store:
	case IT::F32Store:
		printNearOrFar(Bytecode::I32StoreNear, Bytecode::I32StoreFar);
		break;
	case IT::I64Store:
	case IT::F64Store:
		printNearOrFar(Bytecode::I64StoreNear, Bytecode::I64StoreFar);
		break;
	}
}

void WASM::ModuleCompiler::compileMemoryControlInstruction(Instruction instruction)
{
	if (instruction != InstructionType::DataDrop) {
		// Check that the memory at least exists
		memoryByIndex(0);
	}

	switch (instruction.opCode()) {
	case InstructionType::MemorySize: // No popping -> Push once
		pushValue(ValType::I32);
		break;
	case InstructionType::MemoryGrow: // Pop once -> Push once
		popValue(ValType::I32);
		pushValue(ValType::I32);
		break;
	case InstructionType::MemoryFill: // pop thrice
	case InstructionType::MemoryCopy:
	case InstructionType::MemoryInit:
		popValue(ValType::I32);
		popValue(ValType::I32);
		popValue(ValType::I32);
		break;
	}

	// Nullable<Data> data;
	if (instruction == InstructionType::MemoryInit || instruction == InstructionType::DataDrop) {
		// FIXME: Check if the data segment actually exists
		assert(false);
	}

	if (isReachable()) {
		auto bytecode = instruction.toBytecode();
		assert(bytecode.has_value());
		print(*bytecode);

		if (instruction == InstructionType::MemoryInit || instruction == InstructionType::DataDrop) {
			printU32(instruction.dataSegmentIndex());
		}
	}
}

void WASM::ModuleCompiler::compileBranchTableInstruction(Instruction instruction)
{
	const u32 jumpReferencePosition = printedBytecode.size() + 1; // Consider the size of the bytecode -> +1
	auto printJumpAddress = [&](u32 labelIdx, const ControlFrame& frame) {
		if (isReachable()) {
			// Backwards jump
			if (frame.opCode == InstructionType::Loop) {
				i32 distance = frame.bytecodeOffset - jumpReferencePosition;
				printU32(distance);
				return;
			}

			// Forwards jump
			requestAddressPatch(labelIdx, false, false, jumpReferencePosition);
		}
	};

	popValue(ValType::I32);
	auto defaultLabel = instruction.branchTableDefaultLabel();
	if (defaultLabel > controlStack.size()) {
		throwCompilationError("Control stack underflow in branch table default label");
	}

	auto& defaultLabelFrame = controlStack[controlStack.size() - defaultLabel - 1];
	auto defaultLabelTypes = defaultLabelFrame.labelTypes();
	auto defaultArity = defaultLabelTypes.size(module);

	if (!defaultArity.has_value()) {
		throwCompilationError("Default label type references invalid function type");
	}

	auto it = instruction.branchTableVector(currentFunction->expression().bytes());
	auto numLabels = it.nextU32();

	if (isReachable()) {
		print(Bytecode::JumpTable);
		printU32(numLabels);
	}
	
	for (u32 i = 0; i != numLabels; i++) {
		auto label = it.nextU32();
		if (label > controlStack.size()) {
			throwCompilationError("Control stack underflow in branch tabel label");
		}

		auto& labelFrame = controlStack[controlStack.size() - label - 1];
		auto labelTypes = labelFrame.labelTypes();
		auto arity = labelTypes.size(module);
		if (!arity.has_value()) {
			throwCompilationError("Label type references invalid function type");
		}

		if (arity != defaultArity) {
			throwCompilationError("Branch table arity mismatch");
		}

		pushValues(popValuesToList(labelTypes));

		printJumpAddress(label, labelFrame);
	}
	popValues(defaultLabelTypes);

	printJumpAddress(defaultLabel, defaultLabelFrame);

	setUnreachable();
}

void ModuleCompiler::compileInstruction(Instruction instruction, u32 instructionCounter)
{
	auto opCode = instruction.opCode();
	if (opCode.isConstant()
		&& opCode != InstructionType::GlobalGet
		&& opCode != InstructionType::ReferenceFunction
		) {
		compileNumericConstantInstruction(instruction);
		return;
	}

	if (opCode.isUnary()) {
		compileNumericUnaryInstruction(instruction);
		return;
	}

	if (opCode.isBinary()) {
		compileNumericBinaryInstruction(instruction);
		return;
	}

	if (opCode.isMemory()) {
		compileMemoryDataInstruction(instruction);
		return;
	}

	auto validateBlockTypeInstruction = [&]() {
		auto blockType = instruction.blockTypeIndex();
		popValues(blockType.parameters());
		pushControlFrame(instruction.opCode(), blockType);
	};

	auto validateBranchTypeInstruction = [&]() {
		auto label = instruction.branchLabel();
		if (label > controlStack.size() || controlStack.empty()) {
			throwCompilationError("Branch label underflows control frame stack");
		}

		auto& frame = controlStack[controlStack.size() - label - 1];
		auto labelTypes = frame.labelTypes();
		popValues(labelTypes);

		return labelTypes;
	};

	auto printForwardJump = [&](Bytecode shortJump, Bytecode longJump, u32 label, bool isIf) {
		if (isReachable()) {
			// Consider the bytecode not yet printed -> +1
			auto distance = 1+ measureMaxPrintedBlockLength(instructionCounter, label, isIf);
			if (isShortDistance(distance)) {
				print(shortJump);
				requestAddressPatch(label, true, isIf);
			}
			else {
				print(longJump);
				requestAddressPatch(label, false, isIf);
			}
		}
	};

	auto printBranchingJump = [&](Bytecode shortJump, Bytecode longJump) {
		if (!isReachable()) {
			return;
		}

		auto label = instruction.branchLabel();
		auto& frame = controlStack[controlStack.size() - label - 1];

		// Forward jump
		if (frame.opCode != InstructionType::Loop) {
			printForwardJump(shortJump, longJump, label, false);
			return;
		}

		// Consider the bytecode not yet printed -> -1
		i32 distance = frame.bytecodeOffset - printedBytecode.size() -1;
		if (isShortDistance(distance)) {
			print(shortJump);
			printU8(distance);
		}
		else {
			print(longJump);
			printU32(distance);
		}
	};

	auto printGlobalTypeInstruction = [&](Module::ResolvedGlobal global, Bytecode cmd32, Bytecode cmd64) {
		if (isReachable()) {
			u32 numBytes = global.type.valType().sizeInBytes();
			if (numBytes != 4 && numBytes != 8) {
				throwCompilationError("Only globals with 32bit and 64bit are supported");
			}
			print(numBytes == 4 ? cmd32 : cmd64);
			printPointer(&global.instance);
		}
	};

	auto printReturnInstructionForCurrentFunction = [&]() {
		auto resultSpaceInBytes = currentFunction->functionType().resultStackSectionSizeInBytes();
		assert(resultSpaceInBytes % 4 == 0);
		auto resultSpaceInSlots = resultSpaceInBytes / 4;
		if (resultSpaceInSlots <= 255) {
			print(Bytecode::ReturnFew);
			printU8(resultSpaceInSlots);
		}
		else {
			print(Bytecode::ReturnMany);
			printU32(resultSpaceInSlots);
		}
	};

	auto printSelectInstructionIfReachable = [&](ValueRecord firstType, ValueRecord secondType) {
		if (isReachable()) {
			assert(firstType.has_value());
			assert(secondType.has_value());
			assert(*firstType == *secondType);

			if (firstType->sizeInBytes() == 4) {
				print(Bytecode::I32Select);
			}
			else {
				print(Bytecode::I64Select);
			}
		}
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
		printForwardJump(Bytecode::IfFalseJumpShort, Bytecode::IfFalseJumpLong, 0, true);
		return;

	case IT::Else: {
		auto frame = popControlFrame();
		if (frame.opCode != InstructionType::If) {
			throwCompilationError("If block expected before else block");
		}

		// Push the frame for the else-instruction, but transfert the address patch
		// requests instead of processing them, to have them jump behind the else-block
		auto& newFrame = pushControlFrame(InstructionType::Else, frame.blockTypeIndex);
		newFrame.addressPatchList = std::move(frame.addressPatchList);

		// Jump behind the else-block when leaving the if-block
		printForwardJump(Bytecode::JumpShort, Bytecode::JumpLong, 0, false);

		// Patch the address of the jump printed by the if-instruction
		assert(frame.elseLabelAddressPatch.has_value());
		patchAddress(*frame.elseLabelAddressPatch);

		return;
	}

	case IT::End: {
		auto frame = popControlFrame();
		pushValues(frame.blockTypeIndex.results());
		frame.processAddressPatchRequests(*this);

		// Add a return instruction at the end of the function block
		if (controlStack.empty() && !frame.unreachable) {
			auto isEmpty = currentFunction->expression().size() < 2;
			if (isEmpty) {
				printReturnInstructionForCurrentFunction();
			}
			else {
				auto& lastInstruction = *(currentFunction->expression().end() - 2);
				if (lastInstruction != InstructionType::Return) {
					printReturnInstructionForCurrentFunction();
				}
			}
		}
		return;
	}

	case IT::Branch:
		validateBranchTypeInstruction();
		printBranchingJump(Bytecode::JumpShort, Bytecode::JumpLong);
		setUnreachable();
		return;

	case IT::BranchIf: {
		popValue(ValType::I32);
		auto labelTypes = validateBranchTypeInstruction();
		pushValues(labelTypes);
		printBranchingJump(Bytecode::IfTrueJumpShort, Bytecode::IfTrueJumpLong);
		return;
	}

	case IT::Return: {
		if (controlStack.empty()) {
			throwCompilationError("Control stack underflow during return");
		}
		popValues(controlStack[0].blockTypeIndex.results());

		if (isReachable()) {
			printReturnInstructionForCurrentFunction();
		}

		setUnreachable();
		return;
	}

	case IT::BranchTable:
		compileBranchTableInstruction(instruction);
		return;

	case IT::Call: {
		auto functionIdx = instruction.functionIndex();
		auto function = module.functionByIndex(functionIdx);
		assert(function.has_value());
		auto& funcType = function->functionType();
		popValues(funcType.parameters());
		pushValues(funcType.results());
		
		auto bytecodeFunction = function->asBytecodeFunction();
		if (bytecodeFunction.has_value()) {
			// FIXME: Print the pointer to the actual bytecode instead?
			print(Bytecode::Call);
			printPointer(bytecodeFunction.pointer());
			printU32(funcType.parameterStackSectionSizeInBytes());

		}
		else {
			auto hostFunction = function->asHostFunction();
			assert(hostFunction.has_value());

			print(Bytecode::CallHost);
			printPointer(hostFunction.pointer());
		}
		
		return;
	}

	case IT::Drop: {
		auto type = popValue();
		if (type.has_value() && isReachable()) {
			if (type->sizeInBytes() == 4) {
				print(Bytecode::I32Drop);
			}
			else if (type->sizeInBytes() == 8) {
				print(Bytecode::I64Drop);
			}
			else {
				throwCompilationError("Drop instruction only implemented for 32bit and 64bit");
			}
		}
		return;
	}

	case IT::Select: {
		popValue(ValType::I32);
		auto firstType= popValue();
		auto secondType= popValue();

		auto isNum = [](ValueRecord record) {
			// Empty is also a number, so just use I32 as a placeholder
			return record.value_or(ValType::I32).isNumber();
		};

		auto isVec = [](ValueRecord record) {
			// Empty is also a vector, so just use V128 as a placeholder
			return record.value_or(ValType::V128).isVector();
		};

		if (!((isNum(firstType) && isNum(secondType)) || (isVec(firstType) && isVec(secondType)))) {
			throwCompilationError("Select instruction expected either two numbers or two vectors to select from");
		}

		if (firstType.has_value() && secondType.has_value() && *firstType != *secondType) {
			throwCompilationError("Select instruction expected identical types to select from");
		}

		pushMaybeValue(firstType.has_value() ? firstType : secondType);

		printSelectInstructionIfReachable(firstType, secondType);
		return;
	}

	case IT::SelectFrom: {
		auto typeVector = instruction.selectTypeVector(currentFunction->expression().bytes());
		if (typeVector.size() != 1) {
			throwCompilationError("Expected a type vector of size one for SelectFrom instruction");
		}
		auto type = ValType::fromInt(typeVector[0]);

		popValue(ValType::I32);
		popValue(type);
		popValue(type);
		pushValue(type);

		printSelectInstructionIfReachable(type, type);
		return;
	}

	case IT::LocalGet: {
		auto local = localByIndex(instruction.localIndex());
		printLocalGetSetTeeBytecodeIfReachable(
			local,
			Bytecode::I32LocalGetNear,
			Bytecode::I32LocalGetFar,
			Bytecode::I64LocalGetNear,
			Bytecode::I64LocalGetFar
		);
		pushValue(local.type);
		return;
	}

	case IT::LocalSet: {
		auto local = localByIndex(instruction.localIndex());
		popValue(local.type);
		printLocalGetSetTeeBytecodeIfReachable(
			local,
			Bytecode::I32LocalSetNear,
			Bytecode::I32LocalSetFar,
			Bytecode::I64LocalSetNear,
			Bytecode::I64LocalSetFar
		);
		return;
	}

	case IT::LocalTee: {
		auto local = localByIndex(instruction.localIndex());
		popValue(local.type);
		pushValue(local.type);
		printLocalGetSetTeeBytecodeIfReachable(
			local,
			Bytecode::I32LocalTeeNear,
			Bytecode::I32LocalTeeFar,
			Bytecode::I64LocalTeeNear,
			Bytecode::I64LocalTeeFar
		);
		return;
	}

	case IT::GlobalGet: {
		auto global = globalByIndex(instruction.globalIndex());
		pushValue(global.type.valType());

		printGlobalTypeInstruction(global, Bytecode::I32GlobalGet, Bytecode::I64GlobalGet);
		return;
	}

	case IT::GlobalSet: {
		auto global = globalByIndex(instruction.globalIndex());
		if (!global.type.isMutable()) {
			throwCompilationError("Cannot write to immutable global");
		}
		popValue(global.type.valType());

		printGlobalTypeInstruction(global, Bytecode::I32GlobalSet, Bytecode::I64GlobalSet);
		return;
	}

	case IT::MemorySize:
	case IT::MemoryGrow:
	case IT::MemoryFill:
	case IT::MemoryCopy:
	case IT::MemoryInit:
	case IT::DataDrop:
		compileMemoryControlInstruction(instruction);
		return;
	}

	std::cerr << "Compilation not implemented for instruction '" << opCode.name() << "'!" << std::endl;
	throw std::runtime_error{ "Compilation not implemented for instruction" };
}

void WASM::ModuleCompiler::printBytecode(std::ostream& out)
{
	using std::setw, std::hex, std::dec;

	auto it = printedBytecode.iterator();
	u32 idx = 0;
	while (it.hasNext()) {
		auto opCodeAddress = (u64)it.positionPointer();
		out << "  " << setw(3) << idx << ": " << hex << opCodeAddress << "  ";

		auto opCode = Bytecode::fromInt(it.nextU8());
		out << setw(2) << (u32)opCode << " (" << opCode.name() << ")";

		auto args = opCode.arguments();
		if (args.isU64()) {
			for (u32 i = 0; i != args.count(); i++) {
				out << " " << it.nextLittleEndianU64();
			}
		}

		u32 lastU32= 0;
		if (args.isU32()) {
			for (u32 i = 0; i != args.count(); i++) {
				lastU32 = it.nextLittleEndianU32();
				out << " " << lastU32;
			}
		}

		u8 lastU8= 0;
		if (args.isU8()) {
			for (u32 i = 0; i != args.count(); i++) {
				lastU8 = it.nextU8();
				out << " " << (u32)lastU8;
			}
		}

		if (opCode == Bytecode::JumpShort || opCode == Bytecode::IfTrueJumpShort || opCode == Bytecode::IfFalseJumpShort) {
			out << " (-> " << opCodeAddress+ 1 + (i8)lastU8 << ")";
		}
		else if (opCode == Bytecode::JumpLong || opCode == Bytecode::IfTrueJumpLong || opCode == Bytecode::IfFalseJumpLong) {
			out << " (-> " << opCodeAddress+ 1 + (i32)lastU32 << ")";
		}
		else if (opCode == Bytecode::JumpTable) {
			for (u32 i = 0; i != lastU32; i++) {
				out << "\n      (" << setw(2) << i << " -> " << opCodeAddress + 1 + (i32)it.nextLittleEndianU32() << ")";
			}

			out << "\n      (default -> " << opCodeAddress + 1 + (i32)it.nextLittleEndianU32() << ")";
		}

		out << dec << std::endl;
		idx++;
	}
}

void ModuleCompiler::throwCompilationError(const char* msg) const
{
	if (currentFunction) {
		throw CompileError{ module.name(), currentFunction->index(), std::string{msg} };
	}
	throw CompileError{ module.name(), std::string{msg} };
}
