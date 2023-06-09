
#include <cassert>
#include <iomanip>
#include <iostream>

#include "interpreter.h"
#include "introspection.h"
#include "error.h"
#include "virtual_span.h"

using namespace WASM;

static constexpr inline bool isShortDistance(i32 distance) {
	return distance >= -128 && distance <= 127;
}

Nullable<const std::string> Function::lookupName(const Module& module) const
{
	return module.functionNameByIndex(mModuleIndex);
}

BytecodeFunction::BytecodeFunction(ModuleFunctionIndex idx, ModuleTypeIndex ti, const FunctionType& ft, FunctionCode c)
	: Function{ idx }, mModuleTypeIndex{ ti }, type{ft}, code{ std::move(c.code) } {
	uncompressLocalTypes(c.compressedLocalTypes);
}

std::optional<BytecodeFunction::LocalOffset> BytecodeFunction::localOrParameterByIndex(u32 idx) const
{
	if (idx < uncompressedLocals.size()) {
		return uncompressedLocals[idx];
	}

	return {};
}

bool BytecodeFunction::hasLocals() const
{
	return type->parameters().size() < uncompressedLocals.size();
}

u32 BytecodeFunction::localsCount() const
{
	if (!hasLocals()) {
		return 0;
	}

	return uncompressedLocals.size() - type->parameters().size();
}

u32 BytecodeFunction::operandStackSectionOffsetInBytes() const
{
	if (uncompressedLocals.empty()) {
		return SpecialFrameBytes;
	}

	auto& lastLocal = uncompressedLocals.back();
	auto byteOffset= lastLocal.offset + lastLocal.type.sizeInBytes();

	// Manually add the size of RA + FP + SP + MP, if there are only parameters
	if (!hasLocals()) {
		byteOffset += SpecialFrameBytes;
	}

	return byteOffset;
}

u32 BytecodeFunction::localsSizeInBytes() const
{
	if (!hasLocals()) {
		return 0;
	}

	u32 beginLocalsByteOffset = uncompressedLocals[type->parameters().size()].offset;
	u32 endLocalsByteOffset = operandStackSectionOffsetInBytes();

	return endLocalsByteOffset - beginLocalsByteOffset;
}

bool BytecodeFunction::requiresMemoryInstance() const
{
	for (auto& ins : code) {
		if (ins.opCode().requiresMemoryInstance()) {
			return true;
		}
	}

	return false;
}

void BytecodeFunction::uncompressLocalTypes(const std::vector<CompressedLocalTypes>& compressedLocals)
{
	// Count the parameters and locals
	auto& params = type->parameters();
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

	// Leave space for return address, stack, frame and memory pointer
	byteOffset += SpecialFrameBytes;

	// Decompress and put each local
	for (auto& pack : compressedLocals) {
		for (u32 i = 0; i != pack.count; i++) {
			uncompressedLocals.emplace_back(pack.type, byteOffset);
			byteOffset += pack.type.sizeInBytes();
		}
	}
}

FunctionTable::FunctionTable(ModuleTableIndex idx, const TableType& tableType)
	: index{ idx }, mType{ tableType.valType() }, mLimits{ tableType.limits() }
{
	if (grow(mLimits.min(), {}) != 0) {
		throw std::runtime_error{ "Could not init table" };
	}
}

i32 FunctionTable::grow(u32 increase, Nullable<Function> item)
{
	auto oldSize = table.size();
	if (mLimits.max().has_value() && (sizeType)oldSize + increase > *mLimits.max()) {
		return -1;
	}

	try {
		table.reserve(oldSize + increase);
		table.insert(table.end(), increase, item);
		return oldSize;
	}
	catch (std::bad_alloc&) {
		return -1;
	}
}

void FunctionTable::init(const LinkedElement& element, u32 tableOffset, u32 elementOffset, u32 numItems)
{
	// https://webassembly.github.io/spec/core/exec/instructions.html#xref-syntax-instructions-syntax-instr-table-mathsf-table-init-x-y

	auto& refs = element.references();
	if ((sizeType)elementOffset + numItems > refs.size()) {
		throw std::runtime_error{ "Invalid table init: Element access out of bounds" };
	}

	if ((sizeType)tableOffset + numItems > table.size()) {
		throw std::runtime_error{ "Invalid table init: Table row access out of bounds" };
	}

	if (!numItems) {
		return;
	}
	
	auto readIt = refs.begin() + elementOffset;
	auto writeIt = table.begin() + tableOffset;

	for (u32 i = 0; i != numItems; i++) {
		*(writeIt++) = *(readIt++);
	}
}

void WASM::FunctionTable::copy(const FunctionTable& sourceTable, u32 destinationOffset, u32 sourceOffset, u32 numItems)
{
	// https://webassembly.github.io/spec/core/exec/instructions.html#xref-syntax-instructions-syntax-instr-table-mathsf-table-copy-x-y

	if ((sizeType)sourceOffset + numItems > sourceTable.table.size()) {
		throw std::runtime_error{ "Invalid table copy: Source table row access out of bounds" };
	}

	if ((sizeType)destinationOffset + numItems > table.size()) {
		throw std::runtime_error{ "Invalid table copy: Destination table row access out of bounds" };
	}

	if (!numItems) {
		return;
	}

	// The source area lies after the destination area, so even if they overlap the relevant areas
	// are already read before they get overrriden
	if (destinationOffset <= sourceOffset) {
		auto readIt = sourceTable.table.begin() + sourceOffset;
		auto writeIt = table.begin() + destinationOffset;

		for (u32 i = 0; i != numItems; i++) {
			*(writeIt++) = *(readIt++);
		}

		return;
	}

	// The source index lies before the destination area, so if they overlap, the data to read might
	// already have been clobbered/overridden -> Copy in reverse order to prevent this issue
	auto readIt = sourceTable.table.begin() + sourceOffset+ numItems - 1;
	auto writeIt = table.begin() + destinationOffset+ numItems - 1;

	for (u32 i = 0; i != numItems; i++) {
		*(writeIt--) = *(readIt--);
	}
}

void WASM::FunctionTable::fill(Nullable<Function> val, u32 index, u32 numItems)
{
	// https://webassembly.github.io/spec/core/exec/instructions.html#xref-syntax-instructions-syntax-instr-table-mathsf-table-fill-x

	if ((sizeType)index + numItems > table.size()) {
		throw std::runtime_error{ "Invalid table fill: Table row access out of bounds" };
	}

	if (!numItems) {
		return;
	}

	auto writeIt = table.begin() + index;
	for (u32 i = 0; i != numItems; i++) {
		*(writeIt++) = val;
	}
}


sizeType LinkedElement::initTableIfActive(std::span<FunctionTable> tables)
{
	if (mMode != ElementMode::Active) {
		return 0;
	}

	assert(tableIndex < tables.size());
	tables[tableIndex.value].init(*this, tableOffset, 0, mFunctions.size());
	return mFunctions.size();
}

void WASM::LinkedElement::drop()
{
	// Clear the vector and force deallocation
	mFunctions.clear();
	mFunctions.shrink_to_fit();
}

sizeType WASM::LinkedDataItem::initMemoryIfActive(Module& module) const
{
	if (mMode != DataItemMode::Active) {
		return 0;
	}

	// FIXME: Just assume memory index 0 here
	auto memory= module.memoryByIndex(ModuleMemoryIndex{ 0 });
	assert(memory.has_value());

	memory->init(*this, mMemoryOffset, 0, mDataBytes.size());
	return mDataBytes.size();
}

Memory::Memory(ModuleMemoryIndex idx, Limits l)
	: mIndex{ idx }, mLimits{ l } {
	grow(mLimits.min());
}

i32 Memory::grow(i32 pageCountIncrease)
{
	auto oldByteSize = mData.size();
	auto oldPageCount = oldByteSize / PageSize;

	if (mLimits.max().has_value() && (sizeType)oldPageCount + pageCountIncrease > *mLimits.max()) {
		return -1;
	}

	try {
		auto byteSizeIncrease = pageCountIncrease * PageSize;
		mData.reserve(oldByteSize + byteSizeIncrease);
		mData.insert(mData.end(), byteSizeIncrease, 0x00);
		return oldPageCount;
	}
	catch (std::bad_alloc&) {
		return -1;
	}
}

void WASM::Memory::init(const LinkedDataItem& dataItem, u32 memoryOffset, u32 itemOffset, u32 numBytes)
{
	// https://webassembly.github.io/spec/core/exec/instructions.html#xref-syntax-instructions-syntax-instr-memory-mathsf-memory-init-x

	auto& bytes = dataItem.dataBytes();
	if ((sizeType)itemOffset + numBytes > bytes.size()) {
		throw std::runtime_error{ "Invalid memory init: Data item access out of bounds" };
	}

	if ((sizeType)memoryOffset + numBytes > mData.size()) {
		throw std::runtime_error{ "Invalid memory init: Memory access out of bounds" };
	}

	if (!numBytes) {
		return;
	}

	memcpy(pointer(memoryOffset), bytes.begin()+ itemOffset, numBytes);
}

u64 Memory::minBytes() const {
	return mLimits.min() * PageSize;
}

std::optional<u64> Memory::maxBytes() const {
	auto m = mLimits.max();
	if (m.has_value()) {
		return { *m * PageSize };
	}

	return {};
}

sizeType Memory::currentSizeInPages() const
{
	return mData.size() / PageSize;
}

sizeType WASM::Memory::currentSizeInBytes() const
{
	return mData.size();
}


void WASM::ModuleBase::createMemoryBase(const MemoryType& memoryType, ModuleLinker& linker, Nullable<Introspector> introspector)
{
	auto& memories = linker.createMemory();
	mMemoryIndex = InterpreterMemoryIndex{ (u32)memories.size() };

	memories.emplace_back(ModuleMemoryIndex{ 0 }, memoryType.limits());
}

void WASM::ModuleBase::createGlobalsBase(VirtualForwardIterator<DeclaredGlobalBase>& globals, ModuleLinker& linker, Nullable<Introspector> introspector)
{
	// Count the number of 32bit and 64bit globals, assign absolute indices
	u32 num32BitGlobals = 0;
	u32 num64BitGlobals = 0;
	for (auto& global : globals) {
		auto size = global.valType().sizeInBytes();
		if (size == 4) {
			InterpreterGlobalTypedArrayIndex idx{ (u32)linker.currentNumGlobals32() + num32BitGlobals++ };
			global.setIndexInTypedStorageArray(idx);
		}
		else if (size == 8) {
			InterpreterGlobalTypedArrayIndex idx{ (u32)linker.currentNumGlobals64() + num64BitGlobals++ };
			global.setIndexInTypedStorageArray(idx);
		}
		else {
			throw ValidationError{ std::string{name()}, "Only globals with 32bits and 64bits are supported"};
		}
	}

	// Allocate slots for the globals and init them with 0
	auto& globals32bit = linker.createGlobals32(num32BitGlobals);
	mGlobals32.init(globals32bit, num32BitGlobals);
	globals32bit.insert(globals32bit.end(), num32BitGlobals, {});

	auto& globals64bit = linker.createGlobals64(num64BitGlobals);
	mGlobals64.init(globals64bit, num64BitGlobals);
	globals64bit.insert(globals64bit.end(), num64BitGlobals, {});
}

Module::Module(
	Interpreter& i,
	Buffer b,
	std::string p,
	std::string n,
	std::unique_ptr<ParsingState> s,
	ExportTable e
)
	: ModuleBase{ i },
	mPath{ std::move(p) },
	mName{ std::move(n) },
	mData{ std::move(b) },
	compilationData{ std::move(s) },
	exports{ std::move(e) }
{
	assert(compilationData);
	numImportedFunctions = compilationData->importedFunctions().size();
	numImportedTables = compilationData->importedTableTypes().size();
	numImportedMemories = compilationData->importedMemoryTypes().size();
	numImportedGlobals = compilationData->importedGlobalTypes().size();

	functionNameMap = compilationData->releaseFunctionNames();
}


void WASM::Module::createFunctions(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	auto functionCodes = compilationData->releaseFunctionCodes();
	auto& functions = compilationData->functions();
	auto& functionTypes = compilationData->functionTypes();
	auto& bytecodeFunctions = linker.createFunctions(functions.size());

	mFunctions.init(bytecodeFunctions, functions.size());

	for (u32 i = 0; i != functions.size(); i++) {
		ModuleFunctionIndex moduleFunctionIdx{ (u32)(i + numImportedFunctions) };
		auto typeIdx = functions[i];
		assert(typeIdx < functionTypes.size());
		auto& funcType = functionTypes[typeIdx.value];
		auto& funcCode = functionCodes[i];
		bytecodeFunctions.emplace_back(moduleFunctionIdx, typeIdx, funcType, std::move(funcCode));
	}
}

void WASM::Module::createMemory(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	auto& memoryTypes = compilationData->memoryTypes();
	if (!memoryTypes.empty()) {
		createMemoryBase(memoryTypes.front(), linker, introspector);
	}
}

void Module::createTables(ModuleLinker& linker, Nullable<Introspector>)
{
	// Create function table objects
	// Creating elements and populating tables has to be done separately after
	// imports were resolved and linked items were transferred to the interpreter
	auto& tableTypes = compilationData->tableTypes();
	auto& functionTables = linker.createTables(tableTypes.size());

	mTables.init(functionTables, tableTypes.size());

	for (u32 i = 0; i != tableTypes.size(); i++) {
		ModuleTableIndex tableIdx{ (u32)(i + numImportedTables) };
		auto& tableType = tableTypes[i];
		functionTables.emplace_back(tableIdx, tableType);
	}
}

void WASM::Module::initializeInstance(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	assert(compilationData);
	createElementsAndInitTables(linker, introspector);
	createDataItemsAndInitMemory(linker, introspector);
}

void Module::createElementsAndInitTables(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	// Create linked elements
	auto moduleTables = mTables.span(mInterpreter->allTables);
	auto unlinkedElements = compilationData->releaseElements();
	auto& linkedElements = linker.createElements(unlinkedElements.size());

	mElements.init(linkedElements, unlinkedElements.size());

	// FIXME: Use the numRemainingElements count
	sizeType numFunctions = 0;
	sizeType numElements = 0;
	sizeType numRemainingElements = 0;
	for (u32 i = 0; i != unlinkedElements.size(); i++) {
		auto& unlinkedElement = unlinkedElements[i];
		if (unlinkedElement.mode() != ElementMode::Passive) {
			numRemainingElements++;
		}

		ModuleElementIndex elemIdx{ i };
		linkedElements.emplace_back(unlinkedElement.decodeAndLink(elemIdx, *this));
		auto initCount = linkedElements.back().initTableIfActive(moduleTables);

		if (initCount > 0) {
			numFunctions += initCount;
			numElements++;
		}
	}

	if (introspector.has_value()) {
		introspector->onModuleTableInitialized(*this, numElements, numFunctions);
	}
}

void WASM::Module::createDataItemsAndInitMemory(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	// Create linked data items
	auto unlinkedItems = compilationData->releaseDataItems();
	auto& linkedItems = linker.createDataItems(unlinkedItems.size());

	mDataItems.init(linkedItems, unlinkedItems.size());

	// TODO: Add a count of reamining actively used data items to be
	// able to free them during run time
	sizeType numBytes = 0;
	sizeType numDataItems = 0;
	for (u32 i = 0; i != unlinkedItems.size(); i++) {
		auto& unlinkedItem = unlinkedItems[i];

		ModuleDataIndex dataIdx{ i };
		linkedItems.emplace_back(unlinkedItem.decodeAndLink(dataIdx, *mInterpreter, *this));
		auto initCount = linkedItems.back().initMemoryIfActive(*this);

		if (initCount > 0) {
			numBytes += initCount;
			numDataItems++;
		}
	}

	if (introspector.has_value()) {
		introspector->onModuleMemoryInitialized(*this, numDataItems, numBytes);
	}
}

void Module::createGlobals(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	// FIXME: Find something better than this const cast
	auto& globals = const_cast<std::vector<DeclaredGlobal>&>(compilationData->globals());

	VirtualSpan<DeclaredGlobalBase> globalsSpan{ globals };
	TypedVirtualForwardIterator globalsIter{ globalsSpan };
	createGlobalsBase(globalsIter, linker, introspector);
}

void WASM::Module::instantiate(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	assert(compilationData);
	createFunctions(linker, introspector);
	createMemory(linker, introspector);
	createGlobals(linker, introspector);
	createTables(linker, introspector);
}

Nullable<Function> Module::functionByIndex(ModuleFunctionIndex idx)
{
	if (idx < numImportedFunctions) {
		if (compilationData) {
			return compilationData->importedFunctions()[idx.value].resolvedFunction();
		}

		return {};
	}

	idx -= numImportedFunctions;
	auto functions = mFunctions.span(mInterpreter->allFunctions);
	assert(idx < functions.size());
	return functions[idx.value];
}

std::optional<ResolvedGlobal> Module::globalByIndex(ModuleGlobalIndex idx)
{
	if (!compilationData) {
		return {};
	}

	if (idx < numImportedGlobals) {
		auto& importedGlobal = compilationData->importedGlobalTypes()[idx.value];
		auto baseGlobal = importedGlobal.getBase();
		if (!baseGlobal.has_value()) {
			return {};
		}

		return ResolvedGlobal{
			*baseGlobal,
			importedGlobal.globalType()
		};
	}

	idx -= numImportedGlobals;
	assert(idx < compilationData->globals().size());
	auto& declaredGlobal = compilationData->globals()[idx.value];

	assert(declaredGlobal.indexInTypedStorageArray().has_value());
	auto storageIndex = *declaredGlobal.indexInTypedStorageArray();

	auto& globalType = declaredGlobal.type();
	if (globalType.valType().sizeInBytes() == 4) {
		assert(storageIndex < mInterpreter->allGlobals32.size());
		return ResolvedGlobal{ mInterpreter->allGlobals32[storageIndex.value], globalType };
	}

	assert(storageIndex < mInterpreter->allGlobals64.size());
	return ResolvedGlobal{ mInterpreter->allGlobals64[storageIndex.value], globalType };
}

Nullable<Memory> Module::memoryByIndex(ModuleMemoryIndex idx)
{
	if (idx != 0) {
		return {};
	}

	if (numImportedMemories) {
		if (compilationData) {
			assert(compilationData->importedMemoryTypes().size());
			return compilationData->importedMemoryTypes().front().resolvedMemory();
		}

		return {};
	}

	assert(mMemoryIndex.has_value());
	assert(*mMemoryIndex < mInterpreter->allMemories.size());
	return mInterpreter->allMemories[mMemoryIndex->value];
}

Nullable<FunctionTable> Module::tableByIndex(ModuleTableIndex idx)
{
	if (idx < numImportedTables) {
		if (compilationData) {
			return compilationData->importedTableTypes()[idx.value].resolvedTable();
		}

		return {};
	}

	idx -= numImportedTables;
	auto functionTables = mTables.span(mInterpreter->allTables);
	assert(idx < functionTables.size());
	return functionTables[idx.value];
}

Nullable<LinkedElement> WASM::Module::linkedElementByIndex(ModuleElementIndex idx)
{
	auto elements = mElements.span(mInterpreter->allElements);
	if (idx >= elements.size()) {
		return {};
	}

	return elements[idx.value];
}

Nullable<LinkedDataItem> WASM::Module::linkedDataItemByIndex(ModuleDataIndex idx)
{
	auto dataItems = mDataItems.span(mInterpreter->allDataItems);
	if (idx > dataItems.size()) {
		return {};
	}

	return dataItems[idx.value];
}

Nullable<const Function> Module::findFunctionByBytecodePointer(const u8* pointer) const
{
	for (auto& func : mFunctions.constSpan(mInterpreter->allFunctions)) {
		if (func.bytecode().hasInRange(pointer)) {
			return func;
		}
	}

	return {};
}

std::optional<ExportItem> Module::exportByName(const std::string& name, ExportType type) const
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

	return functionByIndex(exp->asFunctionIndex());
}

Nullable<FunctionTable> Module::exportedTableByName(const std::string& name)
{
	auto exp = exportByName(name, ExportType::TableIndex);
	if (!exp.has_value()) {
		return {};
	}

	return tableByIndex(exp->asTableIndex());
}

Nullable<Memory> Module::exportedMemoryByName(const std::string& name)
{
	auto exp = exportByName(name, ExportType::MemoryIndex);
	if (!exp.has_value()) {
		return {};
	}

	return memoryByIndex(exp->asMemoryIndex());
}

std::optional<ResolvedGlobal> Module::exportedGlobalByName(const std::string& name)
{
	auto exp = exportByName(name, ExportType::GlobalIndex);
	if (!exp.has_value()) {
		return {};
	}

	return globalByIndex(exp->asGlobalIndex());
}

Nullable<const std::string> Module::functionNameByIndex(ModuleFunctionIndex functionIdx) const
{
	auto fnd = functionNameMap.find(functionIdx.value);
	if (fnd == functionNameMap.end()) {
		return {};
	}

	return fnd->second;
}

HostModuleBuilder& WASM::HostModuleBuilder::defineGlobal(std::string name, ValType type, u64 initValue, bool isMutable)
{
	auto [elem, didInsert] = mGlobals.emplace(std::move(name), HostGlobal{ GlobalType{type, isMutable}, initValue });
	if (!didInsert) {
		throw std::runtime_error{ "A host global with this name already exists" };
	}

	return *this;
}

HostModuleBuilder& WASM::HostModuleBuilder::defineMemory(std::string name, u32 minSize, std::optional<u32> maxSize)
{
	if (mMemory.has_value()) {
		throw std::runtime_error{ "The host module already has a memory" };
	}

	if (maxSize.has_value()) {
		mMemory.emplace(std::move(name), HostMemory{minSize, *maxSize});
	}
	else {
		mMemory.emplace(std::move(name), HostMemory{minSize});
	}
	
	return *this;
}

HostModule HostModuleBuilder::toModule(Interpreter& interpreter) {
	u32 idx = 0;
	for (auto& function : mFunctions) {
		ModuleFunctionIndex funcIdx{ idx++ };
		function.second->setIndex(funcIdx);
	}

	return HostModule{
		interpreter,
		std::move(mName),
		std::move(mFunctions),
		std::move(mGlobals),
		std::move(mMemory)
	};
}

HostModule::HostModule(
	Interpreter& i,
	std::string n,
	SealedUnorderedMap<std::string, std::unique_ptr<HostFunctionBase>> fs,
	SealedUnorderedMap<std::string, HostGlobal> hg,
	SealedOptional<NamedHostMemory> m
)
	: ModuleBase{ i },
	mName{ std::move(n) },
	mHostFunctions{ std::move(fs) },
	mHostGlobals{ std::move(hg) },
	mHostMemory{ std::move(m) }
{
}

Nullable<Function> HostModule::exportedFunctionByName(const std::string& name) {
	auto fnd= mHostFunctions.find(name);
	if (fnd == mHostFunctions.end()) {
		return {};
	}

	return Nullable<Function>::fromPointer(fnd->second);
}

Nullable<FunctionTable> HostModule::exportedTableByName(const std::string&)
{
	return {};
}

Nullable<Memory> HostModule::exportedMemoryByName(const std::string& name)
{
	if (mHostMemory.has_value() && mHostMemory->name == name) {
		auto instance= mHostMemory->memory.mLinkedInstance;
		assert(instance.has_value());
		return instance;
	}

	return {};
}

std::optional<ResolvedGlobal> HostModule::exportedGlobalByName(const std::string& name)
{
	auto fnd = mHostGlobals.find(name);
	if (fnd == mHostGlobals.end()) {
		return {};
	}

	auto& hostGlobal = fnd->second;
	assert(hostGlobal.mLinkedInstance.has_value());
	
	return ResolvedGlobal{ *hostGlobal.mLinkedInstance, hostGlobal.type() };
}

NonNull<HostGlobal> WASM::HostModule::hostGlobalByName(const std::string& name)
{
	auto fnd = mHostGlobals.find(name);
	if (fnd != mHostGlobals.end()) {
		return { fnd->second };
	}

	throw LookupError{mName, name, "Unknown host global instance"};
}

NonNull<HostMemory> WASM::HostModule::hostMemoryByName(const std::string& name)
{;
	if (mHostMemory.has_value() && mHostMemory->name == name) {
		return { mHostMemory->memory };
	}

	throw LookupError{ mName, name, "Unknown host memory instance" };
}

void WASM::HostModule::instantiate(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	createMemory(linker, introspector);
	createGlobals(linker, introspector);
}

void WASM::HostModule::createMemory(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	if (mHostMemory.has_value()) {
		createMemoryBase(mHostMemory->memory, linker, introspector);
	}
}

void WASM::HostModule::createGlobals(ModuleLinker& linker, Nullable<Introspector> introspector)
{
	TypedVirtualForwardIteratorOf<DeclaredGlobalBase, std::string, HostGlobal> globalsIter{ mHostGlobals };
	createGlobalsBase(globalsIter, linker, introspector);
}

void WASM::HostModule::initializeInstance(ModuleLinker&, Nullable<Introspector>)
{
	// Set the pointer to the memory instance
	if (mHostMemory.has_value()) {
		assert(mMemoryIndex.has_value());
		assert(*mMemoryIndex < mInterpreter->allMemories.size());
		auto& memoryInstance = mInterpreter->allMemories[mMemoryIndex->value];
		mHostMemory->memory.setLinkedInstance(memoryInstance);
	}

	// Set the pointers to each global
	for (auto& globalPair : mHostGlobals) {
		auto& hostGlobal = globalPair.second;
		auto idx = hostGlobal.indexInTypedStorageArray();
		assert(idx.has_value());

		if (hostGlobal.valType().sizeInBytes() == 4) {
			assert(*idx < mInterpreter->allGlobals32.size());
			hostGlobal.setLinkedInstance(mInterpreter->allGlobals32[idx->value]);
		}
		else {
			assert(*idx < mInterpreter->allGlobals64.size());
			hostGlobal.setLinkedInstance(mInterpreter->allGlobals64[idx->value]);
		}
	}
}

void ModuleLinker::link()
{
	if (interpreter.wasmModules.empty()) {
		throw std::runtime_error{ "Nothing to link" };
	}

	if (introspector.has_value()) {
		introspector->onModuleLinkingStart();
	}

	checkModulesLinkStatus();

	instantiateModules();

	buildDeduplicatedFunctionTypeTable();

	storeLinkedItems();

	initializeHostModules();

	countDependencyItems();

	for (auto& module : interpreter.wasmModules) {
		auto& compilationData = *module.compilationData;
		createDependencyItems(module, compilationData.mutateImportedFunctions());
		createDependencyItems(module, compilationData.mutateImportedGlobalTypes());
		createDependencyItems(module, compilationData.mutateImportedTableTypes());
		createDependencyItems(module, compilationData.mutateImportedMemoryTypes());
	}

	linkDependencies();

	initGlobals();
	initializeWasmModules();
	linkMemoryInstances();
	linkStartFunctions();

	if (introspector.has_value()) {
		introspector->onModuleLinkingFinished();
	}
}

void WASM::ModuleLinker::storeLinkedItems()
{
	interpreter.allFunctionTypes = std::move(allFunctionTypes);
	interpreter.allFunctions = std::move(allFunctions);
	interpreter.allTables = std::move(allTables);
	interpreter.allMemories = std::move(allMemories);
	interpreter.allGlobals32 = std::move(allGlobals32);
	interpreter.allGlobals64 = std::move(allGlobals64);
	interpreter.allElements = std::move(allElements);
	interpreter.allDataItems = std::move(allDataItems);
}

std::vector<BytecodeFunction>& WASM::ModuleLinker::createFunctions(u32 numFunctions)
{
	allFunctions.reserve(allFunctions.size() + numFunctions);
	return allFunctions;
}

std::vector<FunctionTable>& WASM::ModuleLinker::createTables(u32 numTables)
{
	allTables.reserve(allTables.size() + numTables);
	return allTables;
}

std::vector<LinkedElement>& WASM::ModuleLinker::createElements(u32 numElements)
{
	allElements.reserve(allElements.size() + numElements);
	return allElements;
}

std::vector<Memory>& WASM::ModuleLinker::createMemory()
{
	return allMemories;
}

std::vector<LinkedDataItem>& WASM::ModuleLinker::createDataItems(u32 numItems)
{
	allDataItems.reserve(allDataItems.size() + numItems);
	return allDataItems;
}

std::vector<Global<u32>>& WASM::ModuleLinker::createGlobals32(u32 numGlobals)
{
	allGlobals32.reserve(allGlobals32.size() + numGlobals);
	return allGlobals32;
}

std::vector<Global<u64>>& WASM::ModuleLinker::createGlobals64(u32 numGlobals)
{
	allGlobals64.reserve(allGlobals64.size() + numGlobals);
	return allGlobals64;
}

sizeType WASM::ModuleLinker::currentNumGlobals32() const
{
	return allGlobals32.size();
}

sizeType WASM::ModuleLinker::currentNumGlobals64() const
{
	return allGlobals64.size();
}

void ModuleLinker::checkModulesLinkStatus() {
	for (auto& module : interpreter.wasmModules) {
		if( !module.needsLinking() ) {
			throwLinkError(module, "<none>", "Module already linked");
		}
	}
}

void ModuleLinker::instantiateModules()
{
	for (auto& module : interpreter.wasmModules) {
		module.instantiate(*this, introspector);
	}

	for (auto& module : interpreter.hostModules) {
		module.instantiate(*this, introspector);
	}
}

void ModuleLinker::throwLinkError(const Module& module, const Imported& item, const char* message) const
{
	throw LinkError{ std::string{module.name()}, item.scopedName(), std::string{message} };
}

void ModuleLinker::throwLinkError(const Module& module, const char* itemName, const char* message) const
{
	throw LinkError{ std::string{module.name()}, itemName, std::string{message} };
}

void ModuleLinker::initGlobals()
{
	// Globals might reference imports from other modules, so they can only be
	// initialized with values after all imorts have been resolved.

	for (auto& module : interpreter.wasmModules) {
		for (auto& declaredGlobal : module.compilationData->globals()) {
			auto initValue= declaredGlobal.initExpression().constantUntypedValue(module);

			auto idx = declaredGlobal.indexInTypedStorageArray();
			assert(idx.has_value());

			if (declaredGlobal.valType().sizeInBytes() == 4) {
				interpreter.allGlobals32[idx->value].set((u32) initValue);
			}
			else {
				interpreter.allGlobals64[idx->value].set(initValue);
			}
		}
	}

	// Host modules init their globals here as well to be consistent

	for (auto& module : interpreter.hostModules) {
		for (auto& globalPair : module.mHostGlobals) {
			auto& hostGlobal = globalPair.second;

			auto idx = hostGlobal.indexInTypedStorageArray();
			assert(idx.has_value());

			if (hostGlobal.valType().sizeInBytes() == 4) {
				interpreter.allGlobals32[idx->value].set((u32)hostGlobal.initValue());
			}
			else {
				interpreter.allGlobals64[idx->value].set(hostGlobal.initValue());
			}
		}
	}
}

void ModuleLinker::initializeHostModules() {
	// Host modules store pointers to their memory and global instances
	// which need to be set when the vectors of items are sealed. Therefore,
	// this method has to run after 'storeLinkedItems' but before 'createDependencyItems'
	// because dependencies on host modules are resolved immediately,
	// which requires the host module to be ready at this point.

	for (auto& module : interpreter.hostModules) {
		module.initializeInstance(*this, introspector);
	}
}

void ModuleLinker::initializeWasmModules()
{
	// Table elements might reference imported functions and therefore
	// can only be populated after resolving imports. The same goes for
	// memory data segments wich might reference an imported memory
	// instance

	for (auto& module : interpreter.wasmModules) {
		module.initializeInstance(*this, introspector);
	}
}

void ModuleLinker::linkMemoryInstances()
{
	// Set the modules memory instance after resolving imports, as the module might
	// import its memory instance from another module.

	for (auto& module : interpreter.wasmModules) {
		auto mem = module.memoryByIndex(ModuleMemoryIndex{ 0 });
		if (mem.has_value()) {
			module.mLinkedMemory= mem;
		}
	}
}

void ModuleLinker::linkStartFunctions()
{
	for (auto& module : interpreter.wasmModules) {
		auto idx= module.compilationData->startFunctionIndex();
		if (idx.has_value()) {
			auto function= module.functionByIndex(*idx);
			if (!function.has_value()) {
				throwLinkError(module, "<start-function>", "Could not find module start function");
			}

			module.mLinkedStartFunction = function;
		}
	}
}

void ModuleLinker::buildDeduplicatedFunctionTypeTable()
{
	auto& modules = interpreter.wasmModules;
	allFunctionTypes.reserve(modules.front().compilationData->functionTypes().size());

	const auto insertDedupedFunctionType = [&](const FunctionType& type) {
		auto findIt = std::find(allFunctionTypes.begin(), allFunctionTypes.end(), type);
		if (findIt == allFunctionTypes.end()) {
			allFunctionTypes.emplace_back(type);
			return InterpreterTypeIndex{ (u32)allFunctionTypes.size() - 1 };
		}
		return InterpreterTypeIndex{ (u32)(findIt - allFunctionTypes.begin()) };
	};

	FunctionType placeholderVoidType;

	std::vector<InterpreterTypeIndex> typeMap;
	for (auto& module : modules) {
		auto& types = module.compilationData->functionTypes();
		typeMap.reserve(types.size());

		// Map each module type index to a interpreter type index by inserting/finding
		// it in the global array
		for (auto& type : types) {
			typeMap.emplace_back(insertDedupedFunctionType(type));
		}

		// Use the map to set the type indices for each function and import based
		// on their module type index
		for (auto& function : module.mFunctions.span(allFunctions)) {
			auto moduleTypeIdx = function.moduleTypeIndex();

			// Only set the placeholder void type for now, as the 'allFunctionTypes' vector may
			// reallocate on subsequent calls to 'insertDedupedFunctionType'. The addresses are
			// patched in the following loop after host module types were inserted as well
			assert(moduleTypeIdx < typeMap.size());
			auto interpreterTypeIdx = typeMap[moduleTypeIdx.value];
			assert(interpreterTypeIdx < allFunctionTypes.size());
			function.setLinkedFunctionType(interpreterTypeIdx, placeholderVoidType);
		}

		auto& functionImports = module.compilationData->mutateImportedFunctions();
		for (auto& functionImport : functionImports) {
			assert(!functionImport.hasInterpreterTypeIndex());
			auto moduleTypeIdx = functionImport.moduleTypeIndex();

			assert(moduleTypeIdx < typeMap.size());
			auto interpreterTypeIdx = typeMap[moduleTypeIdx.value];
			functionImport.interpreterTypeIndex(interpreterTypeIdx);
		}
	}

	// Set type indices of host modules
	for (auto& module : interpreter.hostModules) {
		for (auto& functionPair : module.mHostFunctions) {
			auto& function = *functionPair.second;
			auto interpreterTypeIdx = insertDedupedFunctionType(function.functionType());

			function.setLinkedFunctionType(interpreterTypeIdx);
		}
	}

	// Patch the addresses to the function types based on their interpreter type index,
	// now that 'allFunctionTypes' will not change any more
	for (auto& function : allFunctions) {
		auto interpreterTypeIdx = function.interpreterTypeIndex();
		function.setLinkedFunctionType(interpreterTypeIdx, allFunctionTypes[interpreterTypeIdx.value]);
	}
}

sizeType ModuleLinker::countDependencyItems()
{
	sizeType numSlots = 0;
	for (auto& module : interpreter.wasmModules) {
		numSlots += module.numImportedFunctions;
		numSlots += module.numImportedGlobals;
		numSlots += module.numImportedMemories;
		numSlots += module.numImportedTables;
	}
	unresolvedImports.reserve(numSlots);
	return numSlots;
}

void ModuleLinker::createDependencyItems(const Module& module, VirtualSpan<Imported> importSpan) {
	for (auto& imported : importSpan) {
		auto moduleFnd = interpreter.moduleNameMap.find(imported.module());
		if (moduleFnd == interpreter.moduleNameMap.end()) {
			throwLinkError(module, imported, "Importing from unknown module");
		}

		// Immediately link to host module dependencies, as host module cannot re-export items
		auto hostModule = moduleFnd->second->asHostModule();
		if (hostModule.has_value()) {
			if (!imported.tryResolveFromModuleWithName(*hostModule)) {
				throwLinkError(module, imported, "Importing unkown item from (host) module");
			}

			if (!imported.isTypeCompatible()) {
				throwLinkError(module, imported, "The types of the import and (host) export are incompatible");
			}

			if (introspector.has_value()) {
				introspector->onLinkingDependencyResolved(module, imported);
			}

			assert(imported.isResolved());
			continue;
		}

		// Create dependency item for imports from wasm modules
		auto exportModule = moduleFnd->second->asWasmModule();
		assert(exportModule.has_value());

		auto exportItem = exportModule->exportByName(imported.name(), imported.requiredExportType());
		if (!exportItem.has_value()) {
			throwLinkError(module, imported, "Importing unkown item from module");
		}

		DependencyItem item{ imported, module, *exportModule, *exportItem };
		addDepenencyItem(item);
	}
}

void ModuleLinker::linkDependencies()
{
	// As a worst case only a single item can be linked each iteration, any more
	// iterations would be the result of circular dependencies
	auto maxIterations = unresolvedImports.storedEntries();
	while (!unresolvedImports.isEmpty() && maxIterations-- > 0) {

		std::optional<sizeType> listIterator= listBegin, prevIterator;
		while (listIterator) {
			auto& item = unresolvedImports[*listIterator];
			assert(!item.import->isResolved());

			auto didFind= item.import->tryResolveFromModuleWithIndex(*item.exportingModule, item.exportedItem.mIndex);
			if (!didFind) {
				prevIterator = *listIterator;
				listIterator = unresolvedImports.nextOf(*listIterator);

				// std::cout << "- " << item.importingModule->name() << " " << item.import->module() << "::" << item.import->name() << " not yet resolvable" << std::endl;
				continue;
			}

			if (!item.import->isTypeCompatible()) {
				throwLinkError(*item.importingModule, *item.import, "The types of the import and export are incompatible");
			}

			if (introspector.has_value()) {
				introspector->onLinkingDependencyResolved(*item.importingModule, *item.import);
			}

			auto nextIteratorPos = unresolvedImports.remove(*listIterator, prevIterator);
			if (listIterator == listBegin) {
				listBegin = nextIteratorPos;
			}

			listIterator = nextIteratorPos;
		}
	}

	// If there is anything left there is at least one circular dependency
	if (!unresolvedImports.isEmpty()) {
		assert(listBegin.has_value());
		auto& item = unresolvedImports[*listBegin];
		throwLinkError(*item.importingModule, *item.import, "Found circular dependency involving this dependency item");
	}
}

void ModuleLinker::addDepenencyItem(DependencyItem item)
{
	if (introspector.has_value()) {
		introspector->onAddingLinkingDependency(*item.importingModule, *item.import, item.exportedItem.mIndex);
	}

	if (!listBegin) {
		listBegin = unresolvedImports.add(std::move(item));
	}
	else {
		listBegin = unresolvedImports.add(*listBegin, std::move(item));
	}
}


std::optional<sizeType> ModuleCompiler::LabelTypes::size(const Module& module) const
{
	ModuleTypeIndex typeIndex{ 0 };
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

	assert(module.compilationData);
	if (typeIndex >= module.compilationData->functionTypes().size()) {
		return {};
	}

	auto& functionType = module.compilationData->functionTypes()[typeIndex.value];
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
	for (auto& function : module.mFunctions.span(interpreter.allFunctions)) {
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

	auto typeIdx = function.moduleTypeIndex();
	controlStack.emplace_back(InstructionType::NoOperation, BlockTypeIndex{ BlockType::TypeIndex, typeIdx }, 0, 0, false, 0);

	// Print entry bytecode if the function has any locals or requires the module instance
	auto localsSizeInBytes = function.localsSizeInBytes();
	if (localsSizeInBytes > 0 || function.requiresMemoryInstance()) {
		auto memory = module.memoryByIndex(ModuleMemoryIndex{ 0 });
		assert(memory.has_value());
		auto memoryIdx = interpreter.indexOfMemoryInstance(*memory);
		assert(localsSizeInBytes % 4 == 0);
		print(Bytecode::Entry);
		printU32(memoryIdx.value);
		printU32(localsSizeInBytes / 4);
	}

	u32 insCounter = 0;
	for (auto& ins : function.expression()) {
		compileInstruction(ins, insCounter++);
	}

	assert(maxStackHeightInBytes % 4 == 0);
	maxStackHeightInBytes += localsSizeInBytes;
	function.setMaxStackHeight(maxStackHeightInBytes / 4);
	function.setBytecode(std::move(printedBytecode));

	if (introspector.has_value()) {
		introspector->onCompiledFunction(module, function);
	}
}

void ModuleCompiler::pushValue(ValType type)
{
	valueStack.emplace_back(type);
	stackHeightInBytes += type.sizeInBytes();
	maxStackHeightInBytes = std::max(maxStackHeightInBytes, stackHeightInBytes);
}

void ModuleCompiler::pushMaybeValue(ValueRecord record)
{
	if (record.has_value()) {
		pushValue(*record);
	}
	else {
		valueStack.emplace_back(record);
		assert(!isReachable());
	}
}

void ModuleCompiler::pushValues(std::span<const ValType> types)
{
	valueStack.reserve(valueStack.size() + types.size());
	valueStack.insert(valueStack.end(), types.begin(), types.end());

	for (auto type : types) {
		stackHeightInBytes += type.sizeInBytes();
		maxStackHeightInBytes = std::max(maxStackHeightInBytes, stackHeightInBytes);
	}
}

void ModuleCompiler::pushValues(std::span<const ValueRecord> types)
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
		assert(module.compilationData);
		if (*parameters >= module.compilationData->functionTypes().size()) {
			throwCompilationError("Block type index references invalid function type");
		}
		auto& type = module.compilationData->functionTypes()[parameters->value];
		pushValues(type.parameters());
	}
}

void ModuleCompiler::pushValues(const BlockTypeResults& results)
{
	if (results == BlockType::TypeIndex) {
		assert(module.compilationData);
		if (results.index >= module.compilationData->functionTypes().size()) {
			throwCompilationError("Block type index references invalid function type");
		}
		auto& type = module.compilationData->functionTypes()[results.index.value];
		pushValues(type.results());
		return;
	}

	if (results == BlockType::ValType) {
		auto valType = ValType::fromInt(results.index.value);
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

	auto local = currentFunction->localOrParameterByIndex(idx);
	if (local.has_value()) {
		return *local;
	}

	throwCompilationError("Local index out of bounds");
}

ResolvedGlobal ModuleCompiler::globalByIndex(ModuleGlobalIndex idx) const
{
	auto global = module.globalByIndex(idx);
	if (global.has_value()) {
		return *global;
	}

	throwCompilationError("Global index out of bounds");
}

const FunctionType& ModuleCompiler::blockTypeByIndex(ModuleTypeIndex idx)
{
	assert(module.compilationData);
	if (idx >= module.compilationData->functionTypes().size()) {
		throwCompilationError("Block type index references invalid function type");
	}
	return module.compilationData->functionTypes()[idx.value];
}

const Memory& ModuleCompiler::memoryByIndex(ModuleMemoryIndex idx)
{
	auto memory= module.memoryByIndex(idx);
	if (memory.has_value()) {
		return *memory;
	}

	throwCompilationError("Memory index out of bounds");
}

const LinkedElement& WASM::ModuleCompiler::linkedElementByIndex(ModuleElementIndex idx) const
{
	auto element= module.linkedElementByIndex(idx);
	if (element.has_value()) {
		return *element;
	}

	throwCompilationError("Linked element index out of bounds");
}

const LinkedDataItem& WASM::ModuleCompiler::linkedDataItemByIndex(ModuleDataIndex idx) const
{
	auto dataItem = module.linkedDataItemByIndex(idx);
	if (dataItem.has_value()) {
		return *dataItem;
	}

	throwCompilationError("Data item index out of bounds");
}

u32 ModuleCompiler::measureMaxPrintedBlockLength(u32 startInstruction, u32 labelIdx, bool runToElse) const
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

void ModuleCompiler::requestAddressPatch(u32 labelIdx, bool isNearJump, bool elseLabel, std::optional<u32> jumpReferencePosition)
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

void ModuleCompiler::popValues(std::span<const ModuleCompiler::ValueRecord> expected)
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

void ModuleCompiler::popValues(std::span<const ValType> expected)
{
	for (auto it = expected.rbegin(); it != expected.rend(); it++) {
		popValue(*it);
	}
}

const std::vector<ModuleCompiler::ValueRecord>& ModuleCompiler::popValuesToList(std::span<const ValType> expected)
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
		auto valType = ValType::fromInt(expected.index.value);
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
		auto valType = ValType::fromInt(expected.index.value);
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

bool ModuleCompiler::isReachable() const
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
	//std::cout << "  Printed at " << printedBytecode.size() << " bytecode: " << c.name() << std::endl;
	printedBytecode.appendU8(c);
}

void ModuleCompiler::printU8(u8 x)
{
	//std::cout << "  Printed at " << printedBytecode.size() << " u8: " << (int) x << std::endl;
	printedBytecode.appendU8(x);
}

void ModuleCompiler::printU32(u32 x)
{
	//std::cout << "  Printed at " << printedBytecode.size() << " u32: " << x << std::endl;
	printedBytecode.appendLittleEndianU32(x);
}

void ModuleCompiler::printU64(u64 x)
{
	//std::cout << "  Printed at " << printedBytecode.size() << " u64: " << x << std::endl;
	printedBytecode.appendLittleEndianU64(x);
}

void ModuleCompiler::printF32(f32 f)
{
	//std::cout << "  Printed at " << printedBytecode.size() << " f32: " << f << " as " << reinterpret_cast<u32&>(f) << std::endl;
	printedBytecode.appendLittleEndianU32(reinterpret_cast<u32&>(f));
}

void ModuleCompiler::printF64(f64 f)
{
	//std::cout << "  Printed at " << printedBytecode.size() << " f64: " << f << " as " << reinterpret_cast<u64&>(f) << std::endl;
	printedBytecode.appendLittleEndianU32(reinterpret_cast<u64&>(f));
}

void ModuleCompiler::printPointer(const void* p)
{
	//std::cout << "  Printed pointer: " << reinterpret_cast<u64&>(p) << std::endl;
	printedBytecode.appendLittleEndianU64(reinterpret_cast<u64&>(p));
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

void ModuleCompiler::printLocalGetSetTeeBytecodeIfReachable(
	BytecodeFunction::LocalOffset local,
	Bytecode near32,
	Bytecode far32,
	Bytecode near64,
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

	// Full stack size = current operand stack + function parameter section + RA + FP + SP + MP + function locals
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
		switch (opCode) {
		case InstructionType::I32Const: {
			auto value = instruction.asIF32Constant();
			if (value < 256) {
				print(Bytecode::I32ConstShort);
				printU8(value);
			}
			else {
				print(Bytecode::I32ConstLong);
				printU32(value);
			}
			break;
		}
		case InstructionType::I64Const: {
			auto value = instruction.asIF64Constant();
			if (value < 256) {
				print(Bytecode::I64ConstShort);
				printU8(value);
			}
			else {
				print(Bytecode::I64ConstLong);
				printU64(value);
			}
			break;
		}
		case InstructionType::F32Const:
			print(Bytecode::I32ConstLong);
			printU32(instruction.asIF32Constant());
			break;
		case InstructionType::F64Const:
			print(Bytecode::I64ConstLong);
			printU64(instruction.asIF64Constant());
			break;
		default:
			throwCompilationError("Unknown numeric constant instruction");
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

void ModuleCompiler::compileMemoryControlInstruction(Instruction instruction)
{
	if (instruction != InstructionType::DataDrop) {
		// Check that the memory at least exists
		memoryByIndex(ModuleMemoryIndex{ 0 });
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

	InterpreterLinkedDataIndex dataItemIdx{ 0 };
	if (instruction == InstructionType::MemoryInit || instruction == InstructionType::DataDrop) {
		// Always check if the data item exists, even if code is not reachable
		auto dataItem = linkedDataItemByIndex(instruction.dataSegmentIndex());
		dataItemIdx = interpreter.indexOfLinkedDataItem(dataItem);
	}

	if (isReachable()) {
		auto bytecode = instruction.toBytecode();
		assert(bytecode.has_value());
		print(*bytecode);

		if (instruction == InstructionType::MemoryInit || instruction == InstructionType::DataDrop) {
			printU32(dataItemIdx.value);
		}
	}
}

void ModuleCompiler::compileBranchTableInstruction(Instruction instruction)
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

void ModuleCompiler::compileTableInstruction(Instruction instruction)
{
	auto moduleTableIdx = instruction.tableIndex();
	auto table = module.tableByIndex(moduleTableIdx);
	if (!table.has_value()) {
		throwCompilationError("Table instruction references invalid table index");
	}

	auto interpreterTableIdx = interpreter.indexOfTableInstance(*table);

	using IT = InstructionType;
	InterpreterTableIndex interpreterSourceTableIdx{ 0 };
	if (instruction == IT::TableCopy) {
		auto moduleSourceTableIdx = instruction.sourceTableIndex();
		auto sourceTable = module.tableByIndex(moduleSourceTableIdx);
		if (!sourceTable.has_value()) {
			throwCompilationError("Table instruction references invalid source table index");
		}

		if (table->type() != sourceTable->type()) {
			throwCompilationError("Table copy instruction references tables with incompatible types");
		}

		interpreterSourceTableIdx = interpreter.indexOfTableInstance(*sourceTable);
	}

	InterpreterLinkedElementIndex interpreterElementIdx{ 0 };
	if (instruction == IT::TableInit) {
		auto moduleElementIdx = instruction.elementIndex();
		auto linkedElement = linkedElementByIndex(moduleElementIdx);

		if (table->type() != linkedElement.referenceType()) {
			throwCompilationError("Table init instruction references element with incompatible type");
		}

		interpreterElementIdx = interpreter.indexOfLinkedElement(linkedElement);
	}

	auto bytecode = instruction.toBytecode();
	assert(bytecode.has_value());
	print(*bytecode);
	printU32(interpreterTableIdx.value);

	auto type = table->type();
	switch (instruction.opCode()) {
	case IT::TableGet:
		popValue(ValType::I32);
		pushValue(type);
		break;
	case IT::TableSet:
		popValue(type);
		popValue(ValType::I32);
		break;
	case IT::TableSize:
		pushValue(ValType::I32);
		break;
	case IT::TableGrow:
		popValue(ValType::I32);
		popValue(type);
		pushValue(ValType::I32);
		break;
	case IT::TableFill:
		popValue(ValType::I32);
		popValue(type);
		popValue(ValType::I32);
		break;
	case IT::TableCopy:
		popValue(ValType::I32);
		popValue(ValType::I32);
		popValue(ValType::I32);

		printU32(interpreterSourceTableIdx.value);
		break;
	case IT::TableInit:
		popValue(ValType::I32);
		popValue(ValType::I32);
		popValue(ValType::I32);

		printU32(interpreterElementIdx.value);
		break;
	}
}

void ModuleCompiler::compileInstruction(Instruction instruction, u32 instructionCounter)
{
	auto opCode = instruction.opCode();
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

	auto printGlobalTypeInstruction = [&](ResolvedGlobal global, Bytecode cmd32, Bytecode cmd64) {
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
			auto parameterBytes = funcType.parameterStackSectionSizeInBytes();
			assert(parameterBytes % 4 == 0);

			// FIXME: Print the pointer to the actual bytecode instead?
			print(Bytecode::Call);
			printPointer(bytecodeFunction.pointer());
			printU32(parameterBytes / 4);

		}
		else {
			auto hostFunction = function->asHostFunction();
			assert(hostFunction.has_value());

			print(Bytecode::CallHost);
			printPointer(hostFunction.pointer());
		}

		return;
	}

	case IT::CallIndirect: {
		auto typeIdx = instruction.functionIndex();
		if (typeIdx >= module.compilationData->functionTypes().size()) {
			throwCompilationError("Call indirect instruction references invalid function type");
		}
		auto& funcType = module.compilationData->functionTypes()[typeIdx.value];

		auto moduleTableIdx = instruction.callTableIndex();
		auto table = module.tableByIndex(moduleTableIdx);
		if (!table.has_value()) {
			throwCompilationError("Call indirect instruction references invalid table index");
		}

		if (table->type() != ValType::FuncRef) {
			throwCompilationError("Call indirect instruction references table that is not function reference type");
		}

		auto interpreterTypeIdx = interpreter.indexOfFunctionType(funcType);
		auto interpreterTableIdx = interpreter.indexOfTableInstance(*table);

		popValue(ValType::I32);
		popValues(funcType.parameters());
		pushValues(funcType.results());

		print(Bytecode::CallIndirect);
		printU32(interpreterTableIdx.value);
		printU32(interpreterTypeIdx.value);
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
		auto firstType = popValue();
		auto secondType = popValue();

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

		// FIXME: An immutable global could be replaced with a constant instruction

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

	case IT::ReferenceNull:
		pushValue(ValType::FuncRef);
		if (isReachable()) {
			print(Bytecode::I64ConstLong);
			printU64(0x00);
		}
		return;

	case IT::ReferenceIsNull:
		popValue(ValType::FuncRef);
		pushValue(ValType::I32);
		if (isReachable()) {
			print(Bytecode::I64EqualZero);
		}
		return;

	case IT::ReferenceFunction: {
		pushValue(ValType::FuncRef);
		auto function = module.functionByIndex(instruction.functionIndex());
		if (!function.has_value()) {
			throwCompilationError("ReferenceFunction instruction reference invalid function index");
		}
		if (isReachable()) {
			// FIXME: Put the actual bytecode address instead of the function instance?
			print(Bytecode::I64ConstLong);
			printPointer(function.pointer());
		}
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

	case IT::I32Const:
	case IT::I64Const:
	case IT::F32Const:
	case IT::F64Const:
		compileNumericConstantInstruction(instruction);
		return;

	case IT::TableGet:
	case IT::TableSet:
	case IT::TableSize:
	case IT::TableGrow:
	case IT::TableFill:
	case IT::TableCopy:
	case IT::TableInit:
		compileTableInstruction(instruction);
		return;

	case IT::ElementDrop: {
		auto moduleElementIdx = instruction.elementIndex();
		auto element = linkedElementByIndex(moduleElementIdx);
		auto interpreterElementIdx = interpreter.indexOfLinkedElement(element);

		print(Bytecode::ElementDrop);
		printU32(interpreterElementIdx.value);
		return;
	}
	}

	std::cerr << "Compilation not implemented for instruction '" << opCode.name() << "'!" << std::endl;
	throw std::runtime_error{ "Compilation not implemented for instruction" };
}

void ModuleCompiler::printBytecode(std::ostream& out) {
	printBytecode(out, printedBytecode);
}

void ModuleCompiler::printBytecode(std::ostream& out, const Buffer& bytecodeBuffer)
{
	using std::setw, std::hex, std::dec;

	// FIXME: Allow for const buffer iteration
	auto it = const_cast<Buffer&>(bytecodeBuffer).iterator();
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
		throw CompileError{ std::string{module.name()}, currentFunction->moduleIndex(), std::string{msg} };
	}
	throw CompileError{ std::string{module.name()}, std::string{msg} };
}
