

#include <iostream>
#include <iomanip>
#include <cassert>

#include "interpreter.h"
#include "introspection.h"
#include "bytecode.h"

using namespace WASM;

HostFunctionBase::HostFunctionBase(ModuleFunctionIndex idx, FunctionType ft)
	: Function{ idx }, mFunctionType { std::move(ft) } {}

void HostFunctionBase::print(std::ostream& out) const {
	out << "Host function: ";
	mFunctionType.print(out);
}

Interpreter::Interpreter() = default;
Interpreter::~Interpreter() = default;

void Interpreter::loadModule(std::string path)
{
	// Loading another module might cause a reallocation in the modules vector, which
	// would invalidate all the addresses in the bytecode
	if (hasLinked) {
		throw std::runtime_error{ "Cannot load module after linking step" };
	}

	auto introspector = Nullable<Introspector>::fromPointer(attachedIntrospector);

	auto buffer = Buffer::fromFile(path);
	ModuleParser parser{ introspector };
	parser.parse(std::move(buffer), std::move(path));

	ModuleValidator validator{ introspector };
	validator.validate(parser);

	wasmModules.emplace_back(parser.toModule(*this));
	auto& module = wasmModules.back();
	registerModuleName(module);
}

void WASM::Interpreter::registerHostModule(HostModule hostModule)
{
	// See: loadModule()
	if (hasLinked) {
		throw std::runtime_error{ "Cannot register (host) module after linking step" };
	}

	hostModules.emplace_back(std::move(hostModule));
	auto& module = hostModules.back();
	registerModuleName(module);
}

void Interpreter::compileAndLinkModules()
{
	if (hasLinked) {
		throw std::runtime_error{ "Already linked" };
	}

	{
		auto introspector = Nullable<Introspector>::fromPointer(attachedIntrospector);
		ModuleLinker linker{ *this, introspector };
		linker.link();
	}

	for (auto& module : wasmModules) {
		auto introspector = Nullable<Introspector>::fromPointer(attachedIntrospector);
		ModuleCompiler compiler{ *this, module };
		compiler.compile();
	}

	hasLinked = true;
}

void Interpreter::runStartFunctions()
{
	for (auto& module : wasmModules) {
		auto startFunction = module.startFunction();
		if (startFunction.has_value()) {
			executeFunction(*startFunction, {});
		}
	}

	/*auto func = wasmModules.front().functionByIndex(0);
	assert(func.has_value() && func->asBytecodeFunction().has_value());

	std::array<Value, 1> args{ Value::fromType<i32>(4) };
	executeFunction(*func, args);*/
}

void Interpreter::attachIntrospector(std::unique_ptr<Introspector> introspector)
{
	attachedIntrospector = std::move(introspector);
}

void Interpreter::registerModuleName(NonNull<ModuleBase> module)
{
	auto result = moduleNameMap.emplace(module->name(), module);
	if (!result.second) {
		if (wasmModules.size() && &wasmModules.back() == module) {
			wasmModules.pop_back();
		}
		else if (hostModules.size() && &hostModules.back() == module) {
			hostModules.pop_back();
		}

		throw std::runtime_error{ "Module name collision" };
	}

	if (attachedIntrospector) {
		attachedIntrospector->onRegisteredModule(*module);
	}
}

ValuePack Interpreter::executeFunction(Function& function, std::span<Value> values)
{
	assert(!isInterpreting);

	auto bytecodeFunction = function.asBytecodeFunction();
	if (bytecodeFunction.has_value()) {
		if(!bytecodeFunction->functionType().takesValuesAsParameters(values)) {
			throw std::runtime_error("Invalid arguments provided to bytecode function");
		}

		auto pack= runInterpreterLoop(*bytecodeFunction, values);

		pack.print(std::cout);

		return pack;
	}

	return ValuePack{ function.functionType(), true, {} };
}

Nullable<Function> Interpreter::findFunction(const std::string& moduleName, const std::string& functionName)
{
	auto moduleFind = moduleNameMap.find(moduleName);
	if (moduleFind == moduleNameMap.end()) {
		return {};
	}

	return moduleFind->second->exportedFunctionByName(functionName);
}

InterpreterTypeIndex Interpreter::indexOfFunctionType(const FunctionType& funcType) const
{
	auto idx = allFunctionTypes.indexOfPointer(&funcType);
	if (idx.has_value()) {
		return InterpreterTypeIndex{ (u32)*idx };
	}

	auto beginIt = allFunctionTypes.begin();
	auto findIt = std::find(beginIt, allFunctionTypes.end(), funcType);
	assert(findIt != allFunctionTypes.end());
	return InterpreterTypeIndex{ (u32)(findIt - beginIt) };
}

InterpreterFunctionIndex WASM::Interpreter::indexOfFunction(const BytecodeFunction& function) const
{
	auto idx = allFunctions.indexOfPointer(&function);
	assert(idx.has_value());
	return InterpreterFunctionIndex{ (u32)*idx };
}

InterpreterMemoryIndex WASM::Interpreter::indexOfMemoryInstance(const Memory& memory) const
{
	auto idx = allMemories.indexOfPointer(&memory);
	assert(idx.has_value());
	return InterpreterMemoryIndex{ (u32)*idx };
}

InterpreterTableIndex WASM::Interpreter::indexOfTableInstance(const FunctionTable& table)
{
	auto idx = allTables.indexOfPointer(&table);
	assert(idx.has_value());
	return InterpreterTableIndex{ (u32)*idx };
}

InterpreterLinkedElementIndex WASM::Interpreter::indexOfLinkedElement(const LinkedElement& elem)
{
	auto idx = allElements.indexOfPointer(&elem);
	assert(idx.has_value());
	return InterpreterLinkedElementIndex{ (u32)*idx };
}

InterpreterLinkedDataIndex WASM::Interpreter::indexOfLinkedDataItem(const LinkedDataItem& item)
{
	auto idx = allDataItems.indexOfPointer(&item);
	assert(idx.has_value());
	return InterpreterLinkedDataIndex{ (u32)*idx };
}

void Interpreter::initState(const BytecodeFunction& function)
{
	if (mStackBase) {
		return;
	}

	// FIXME: Do not hard code the stack size
	mStackBase = std::make_unique<u32[]>(4096);

	mInstructionPointer = function.bytecode().begin();
	mStackPointer = mStackBase.get();
	mFramePointer = mStackBase.get();
	mMemoryPointer = nullptr;
}

void Interpreter::saveState(const u8* ip, u32* sp, u32* fp, Memory* mp)
{
	mInstructionPointer = ip;
	mStackPointer = sp;
	mFramePointer = fp;
	mMemoryPointer = mp;
}

void Interpreter::dumpStack(std::ostream& out) const
{
	auto framePointer = mFramePointer;
	auto stackPointer = mStackPointer;
	auto memoryPointer = mMemoryPointer;
	auto instructionPointer = mInstructionPointer;

	// Count the number of stack frames first
	u32 frameCount = 0;
	while (framePointer) {
		frameCount++;
		framePointer = *(reinterpret_cast<u32**>(framePointer) + 1);
	}

	out << std::hex;

	// Print each stack frame
	framePointer = mFramePointer;
	auto frameIdx = frameCount;
	while (framePointer) {
		auto prevInstructionPointer= *(reinterpret_cast<u8**>(framePointer) + 0);
		auto prevFramePointer= *(reinterpret_cast<u32**>(framePointer) + 1);
		auto prevStackPointer= *(reinterpret_cast<u32**>(framePointer) + 2);
		auto prevMemoryPointer = *(reinterpret_cast<Memory**>(framePointer) + 3);

		out << "Frame " << --frameIdx;
		if (frameIdx == frameCount - 1) {
			out << " (top)";
		}
		else if (!frameIdx) {
			out << " (bottom)";
		}

		out << " FP: " << framePointer << " SP: " << stackPointer << " MP: " << memoryPointer << std::endl;
		auto lookup= findFunctionByBytecodePointer(instructionPointer);
		if (!lookup.has_value()) {
			out << "Stack corruption error: Unknown function for address: " << (u64)instructionPointer << std::endl;
			return;
		}

		auto bytecodeFunction = lookup->function.asBytecodeFunction();
		auto functionName = lookup->function.lookupName(lookup->module);
		if (bytecodeFunction.has_value()) {
			out << "Function: " << bytecodeFunction->moduleIndex() << " at " << bytecodeFunction.pointer();
			if (functionName.has_value()) {
				out << " (" << *functionName << ")";
			}

			auto numParameters = bytecodeFunction->functionType().parameters().size();
			auto numLocals = bytecodeFunction->localsCount();

			out << " Parameters: " << numParameters;
			out << " Locals: " << numLocals;
			out << " Results: " << bytecodeFunction->functionType().results().size() << std::endl;

			u32 stackPointerOffset = 0;
			auto printSingleStackSlot = [&](const char* const name) {
				out << "  " << (u64)--stackPointer << " (-" << std::setw(2) << ++stackPointerOffset << ") " << name << ": " << *stackPointer << std::endl;
			};

			auto printDoubleStackSlot = [&](const char* const name) {
				out << "  " << (u64)--stackPointer << " (-" << std::setw(2) << ++stackPointerOffset << ")" << std::endl;
				out << "  " << (u64)--stackPointer << " (-" << std::setw(2) << ++stackPointerOffset << ") " << name << ": " << *reinterpret_cast<u64**>(stackPointer) << std::endl;
			};

			auto printTypedLocals = [&](const char* const name, i32 endIdx, i32 beginIdx) {
				for (i32 i = endIdx - 1; i >= beginIdx; i--) {
					auto localOffset = bytecodeFunction->localOrParameterByIndex(i);
					assert(localOffset.has_value());
					if (localOffset->type.sizeInBytes() == 4) {
						printSingleStackSlot(name);
					}
					else if (localOffset->type.sizeInBytes() == 8) {
						printDoubleStackSlot(name);
					}
					else {
						out << "Only types with 32bit or 64bit are supported" << std::endl;
					}
				}
			};
			
			auto operandSlotsEnd = prevStackPointer + bytecodeFunction->operandStackSectionOffsetInBytes()/4;
			while (stackPointer > operandSlotsEnd) {
				printSingleStackSlot("Operand");
			}

			printTypedLocals("Local", numLocals+ numParameters, numParameters);
			
			printDoubleStackSlot("   MP");
			printDoubleStackSlot("   SP");
			printDoubleStackSlot("   FP");
			printDoubleStackSlot("   RA");

			printTypedLocals("Param", numParameters, 0);
		}
		else {
			out << "Host functions not supported for dumping" << std::endl;
			return;
		}

		instructionPointer = prevInstructionPointer;
		framePointer = prevFramePointer;
		stackPointer = prevStackPointer;
		memoryPointer = prevMemoryPointer;
	}

	out << std::dec;
}

std::optional<Interpreter::FunctionLookup> Interpreter::findFunctionByBytecodePointer(const u8* bytecodePointer) const
{
	for (auto& module : wasmModules) {
		auto function = module.findFunctionByBytecodePointer(bytecodePointer);
		if (function.has_value()) {
			return FunctionLookup{*function, module};
		}
	}

	return {};
}

ValuePack Interpreter::runInterpreterLoop(const BytecodeFunction& function, std::span<Value> parameters)
{
	assert(!isInterpreting);
	isInterpreting = true;

	initState(function);
	const u8* instructionPointer = mInstructionPointer;
	u32* stackPointer = mStackPointer;
	u32* framePointer = mFramePointer;
	Memory* memoryPointer = mMemoryPointer;

	auto loadOperandU32 = [&]() -> u32 [[msvc::forceinline]] {
		u32 operand = *reinterpret_cast<const u32*>(instructionPointer);
		instructionPointer += 4;
		return operand;
	};

	auto loadOperandU64 = [&]() -> u64 [[msvc::forceinline]] {
		u64 operand = *reinterpret_cast<const u64*>(instructionPointer);
		instructionPointer += 8;
		return operand;
	};

	auto loadOperandPtr = [&]() [[msvc::forceinline]] {
		void* operand = *reinterpret_cast<void*const*>(instructionPointer);
		instructionPointer += 8;
		return operand;
	};

	auto pushU32 = [&](u32 val) [[msvc::forceinline]] {
		*(stackPointer++) = val;
	};

	auto pushU64 = [&](u64 val) [[msvc::forceinline]] {
		*reinterpret_cast<u64*>(stackPointer) = val;
		stackPointer += 2;
	};

	auto pushPtr = [&](const void* ptr) [[msvc::forceinline]] {
		*reinterpret_cast<const void**>(stackPointer) = ptr;
		stackPointer += 2;
	};

	auto popU32 = [&]() -> u32 [[msvc::forceinline]] {
		return *(--stackPointer);
	};

	auto popU64 = [&]() -> u64 [[msvc::forceinline]] {
		stackPointer -= 2;
		return *reinterpret_cast<u64*>(stackPointer);
	};

	auto loadPtrWithFrameOffset = [&](u32 offset) -> void* [[msvc::forceinline]] {
		return reinterpret_cast<void**>(framePointer)[offset];
	};

	auto loadU64WithStackOffset = [&](u32 offset) -> u64 [[msvc::forceinline]] {
		return *reinterpret_cast<u64*>(stackPointer - offset);
	};

	auto storeU64WithStackOffset = [&](u32 offset, u64 value) -> void [[msvc::forceinline]] {
		*reinterpret_cast<u64*>(stackPointer - offset)= value;
	};

	auto doBytecodeFunctionCall = [&](BytecodeFunction * callee, u32 stackParameterSection) -> void [[msvc::forceinline]] {
		auto stackPointerToSave = stackPointer - stackParameterSection;
		auto newFramePointer = stackPointer;

		if (callee->maxStackHeight() + stackPointer > mStackBase.get() + 4069) {
			throw std::runtime_error{ "Stack overflow" };
		}

		pushPtr(instructionPointer);
		pushPtr(framePointer);
		pushPtr(stackPointerToSave);
		pushPtr(memoryPointer);

		// FIXME: This value should not need to be calculated
		for (u32 i = 0; i != callee->localsSizeInBytes() / 4; i++) {
			pushU32(0);
		}

		framePointer = newFramePointer;
		instructionPointer = callee->bytecode().begin();
		memoryPointer = nullptr;
	};

	// Check stack
	assert(function.maxStackHeight() < 4096);

	// Push parameters to stack
	for (auto& parameter : parameters) {
		auto numBytes = parameter.sizeInBytes();
		if ( numBytes == 4) {
			pushU32(parameter.asU32());
		}
		else if (numBytes == 8) {
			pushU64(parameter.asU64());
		}
		else {
			throw std::runtime_error{ "Only 32bit and 64bit values are supported" };
		}
	}

	framePointer = stackPointer; // Put FP after the parameters

	// Push frame data to stace -> RA, FP, SP, MP
	pushPtr(0x00);
	pushPtr(0x00);
	pushPtr(mStackBase.get());
	pushPtr(memoryPointer);

	u64 opA, opB, opC;

	using BC = Bytecode;
	while (true) {
		auto bytecode = *(instructionPointer++);
		std::cout << std::hex << (u64)(instructionPointer- 1) << " Executing bytecode " << std::dec << Bytecode::fromInt(bytecode).name() << std::endl;

		switch (bytecode) {
		case BC::Unreachable:
			throw std::runtime_error{ "unreachable code" };
		case BC::JumpShort: {
			i8 offset = *(instructionPointer++);
			instructionPointer -= 1;
			instructionPointer += offset;
			continue;
		}
		case BC::JumpLong: {
			i32 offset = loadOperandU32();
			instructionPointer -= 4;
			instructionPointer += offset;
			continue;
		}
		case BC::IfTrueJumpShort: {
			i8 offset = *(instructionPointer++);
			opA = popU32();
			if (opA) {
				instructionPointer -= 1;
				instructionPointer += offset;
			}
			continue;
		}
		case BC::IfTrueJumpLong: {
			i32 offset = loadOperandU32();
			opA = popU32();
			if (opA) {
				instructionPointer -= 4;
				instructionPointer += offset;
			}
			continue;
		}
		case BC::IfFalseJumpShort: {
			i8 offset = *(instructionPointer++);
			opA = popU32();
			if (!opA) {
				instructionPointer -= 1;
				instructionPointer += offset;
			}
			continue;
		}
		case BC::IfFalseJumpLong: {
			i32 offset = loadOperandU32();
			opA = popU32();
			if (!opA) {
				instructionPointer -= 4;
				instructionPointer += offset;
			}
			continue;
		}
		case BC::JumpTable:
			opA = loadOperandU32();
			opB = popU32();
			if (opB > opA) {
				opB = opA;
			}
			instructionPointer += reinterpret_cast<const i32*>(instructionPointer)[opB] - 4;
			continue;
		case BC::ReturnFew: {
			auto numSlotsToReturn = *(instructionPointer++);
			auto currentStackPointer = stackPointer;
			instructionPointer = (u8*)loadPtrWithFrameOffset(0);
			auto oldFramePointer = (u32*)loadPtrWithFrameOffset(1);
			stackPointer = (u32*)loadPtrWithFrameOffset(2);
			memoryPointer = (Memory*)loadPtrWithFrameOffset(3);
			framePointer = oldFramePointer;

			for (i64 i = 0; i != numSlotsToReturn; i++) {
				pushU32(currentStackPointer[i-numSlotsToReturn]);
			}

			if (!instructionPointer) {
				std::cout << "Execution finished" << std::endl;
				return ValuePack{ function.functionType(), true, {mStackBase.get(), (sizeType)(stackPointer - mStackBase.get())} };
			}
			continue;
		}
		case BC::ReturnMany:
			break;
		case BC::Call: {
			auto callee = (BytecodeFunction*)loadOperandPtr();
			auto stackParameterSection = loadOperandU32();
			doBytecodeFunctionCall(callee, stackParameterSection);
			continue;
		}
		case BC::CallIndirect:  {
			auto functionIdx = popU32();
			auto tableIdx = loadOperandU32();
			auto typeIdx = loadOperandU32();
			assert(tableIdx < allTables.size());
			assert(typeIdx < allFunctionTypes.size());

			auto& table = allTables[tableIdx];
			auto function= table.at(functionIdx);
			if (!function.has_value()) {
				throw std::runtime_error("Invalid indirect call to null");
			}
			if (function->interpreterTypeIndex() != typeIdx) {
				throw std::runtime_error("Invalid indirect call to mismatched function type");
			}
			auto hostFunction = function->asHostFunction();
			if (hostFunction.has_value()) {
				stackPointer = hostFunction->executeFunction(stackPointer);
				continue;
			}
			auto bytecodeFunction = reinterpret_cast<BytecodeFunction*>(function.pointer());
			auto stackParameterSection= bytecodeFunction->functionType().parameterStackSectionSizeInBytes() / 4;
			doBytecodeFunctionCall(bytecodeFunction, stackParameterSection);
			continue;
		}
		case BC::CallHost: {
			auto callee = (HostFunctionBase*)loadOperandPtr();
			stackPointer= callee->executeFunction(stackPointer);
			continue;
		}
		case BC::Entry: {
			auto memoryIdx = loadOperandU32();
			memoryPointer = &allMemories[memoryIdx];

			auto numLocals = loadOperandU32();
			while (numLocals-- > 0) {
				pushU32(0);
			}
			continue;
		}
		case BC::I32Drop:
			stackPointer--;
			instructionPointer++;
			continue;
		case BC::I64Drop:
			stackPointer -= 2;
			instructionPointer++;
			continue;
		case BC::I32Select:
			opC = popU32();
			opB = popU32();
			opA = popU32();
			pushU32( opC ? opA : opB );
			continue;
		case BC::I64Select:
			opC = popU32();
			opB = popU64();
			opA = popU64();
			pushU64(opC ? opA : opB);
			continue;
		case BC::I32LocalGetFar:
			opA = loadOperandU32();
			pushU32(stackPointer[-opA]);
			continue;
		case BC::I32LocalSetFar:
			opA = loadOperandU32();
			stackPointer[-opA] = popU32();
			continue;
		case BC::I32LocalTeeFar:
			opA = loadOperandU32();
			opB = stackPointer[-1];
			stackPointer[-opA] = opB;
			continue;
		case BC::I32LocalGetNear:
			opA = *(instructionPointer++);
			pushU32(stackPointer[-opA]);
			continue;
		case BC::I32LocalSetNear:
			opA = *(instructionPointer++);
			stackPointer[-opA]= popU32();
			continue;
		case BC::I32LocalTeeNear:
			opA = *(instructionPointer++);
			opB = stackPointer[-1];
			stackPointer[-opA] = opB;
			continue;
		case BC::I64LocalGetFar:
			opA = loadOperandU32();
			pushU64(loadU64WithStackOffset(opA));
			continue;
		case BC::I64LocalSetFar:
			opA = loadOperandU32();
			storeU64WithStackOffset(opA, popU64());
			continue;
		case BC::I64LocalTeeFar:
			opA = loadOperandU32();
			opB = loadU64WithStackOffset(2);
			storeU64WithStackOffset(opA, opB);
			continue;
		case BC::I64LocalGetNear:
			opA = *(instructionPointer++);
			pushU64(loadU64WithStackOffset(opA));
			continue;
		case BC::I64LocalSetNear:
			opA = *(instructionPointer++);
			storeU64WithStackOffset(opA, popU64());
			continue;
		case BC::I64LocalTeeNear:
			opA = *(instructionPointer++);
			opB = loadU64WithStackOffset(2);
			storeU64WithStackOffset(opA, opB);
			continue;
		case BC::I32GlobalGet: {
			auto ptr = (u32*)loadOperandPtr();
			pushU32(*ptr);
			continue;
		}
		case BC::I32GlobalSet: {
			auto ptr = (u32*)loadOperandPtr();
			*ptr = popU32();
			continue;
		}
		case BC::I64GlobalGet: {
			auto ptr = (u64*)loadOperandPtr();
			pushU64(*ptr);
			continue;
		}
		case BC::I64GlobalSet: {
			auto ptr = (u64*)loadOperandPtr();
			*ptr = popU32();
			continue;
		}
		case BC::TableGet:
		case BC::TableSet:
		case BC::TableInit:
		case BC::ElementDrop:
		case BC::TableCopy:
		case BC::TableGrow:
		case BC::TableSize:
		case BC::TableFill:
		case BC::I32LoadNear:
		case BC::I64LoadNear:
		case BC::I32LoadFar:
		case BC::I64LoadFar:
		case BC::I32Load8s:
		case BC::I32Load8u:
		case BC::I32Load16s:
		case BC::I32Load16u:
		case BC::I64Load8s:
		case BC::I64Load8u:
		case BC::I64Load16s:
		case BC::I64Load16u:
		case BC::I64Load32s:
		case BC::I64Load32u:
			break;
		case BC::I32StoreNear:
			assert(memoryPointer);
			opC = *(instructionPointer++);
			opB = popU32();
			opA = popU32();
			*reinterpret_cast<u32*>(memoryPointer->pointer(opC + opA)) = opB;
			continue;
		case BC::I64StoreNear:
		case BC::I32StoreFar:
		case BC::I64StoreFar:
		case BC::I32Store8:
		case BC::I32Store16:
		case BC::I64Store8:
		case BC::I64Store16:
		case BC::I64Store32:
			break;
		case BC::MemorySize: {
			assert(memoryPointer);
			pushU32(memoryPointer->currentSize());
			continue;
		}
		case BC::MemoryGrow:
		case BC::MemoryInit:
		case BC::DataDrop:
		case BC::MemoryCopy:
		case BC::MemoryFill:
		case BC::I32ConstShort:
			pushU32(*(instructionPointer++));
			continue;
		case BC::I32ConstLong:
			pushU32(loadOperandU32());
			continue;
		case BC::I64ConstShort:
			pushU64(*(instructionPointer++));
			continue;
		case BC::I64ConstLong:
			pushU32(loadOperandU64());
			continue;
		case BC::I32EqualZero:
			opA = popU32();
			pushU32(opA == 0);
			continue;
		case BC::I32Equal:
			opA = popU32();
			opB = popU32();
			pushU32(opA == opB);
			continue;
		case BC::I32NotEqual:
			opA = popU32();
			opB = popU32();
			pushU32(opA != opB);
			continue;
		case BC::I32LesserS:
			opB = popU32();
			opA = popU32();
			pushU32((i32)opA < (i32)opB);
			continue;
		case BC::I32LesserU:
			opB = popU32();
			opA = popU32();
			pushU32(opA < opB);
			continue;
		case BC::I32GreaterS:
			opB = popU32();
			opA = popU32();
			pushU32((i32)opA > (i32)opB);
			continue;
		case BC::I32GreaterU:
			opB = popU32();
			opA = popU32();
			pushU32(opA > opB);
			continue;
		case BC::I32LesserEqualS:
			opB = popU32();
			opA = popU32();
			pushU32((i32)opA <= (i32)opB);
			continue;
		case BC::I32LesserEqualU:
			opB = popU32();
			opA = popU32();
			pushU32(opA <= opB);
			continue;
		case BC::I32GreaterEqualS:
			opB = popU32();
			opA = popU32();
			pushU32((i32)opA >= (i32)opB);
			continue;
		case BC::I32GreaterEqualU:
			opB = popU32();
			opA = popU32();
			pushU32(opA >= opB);
			continue;
		case BC::I64EqualZero:
			opA = popU64();
			pushU32(opA == 0);
			continue;
		case BC::I64Equal:
			opA = popU64();
			opB = popU64();
			pushU32(opA == opB);
			continue;
		case BC::I64NotEqual:
			opA = popU64();
			opB = popU64();
			pushU32(opA != opB);
			continue;
		case BC::I64LesserS:
			opB = popU64();
			opA = popU64();
			pushU32((i64)opA < (i64)opB);
			continue;
		case BC::I64LesserU:
			opB = popU64();
			opA = popU64();
			pushU32(opA < opB);
			continue;
		case BC::I64GreaterS:
			opB = popU64();
			opA = popU64();
			pushU32((i64)opA > (i64)opB);
			continue;
		case BC::I64GreaterU:
			opB = popU64();
			opA = popU64();
			pushU32(opA > opB);
			continue;
		case BC::I64LesserEqualS:
			opB = popU64();
			opA = popU64();
			pushU32((i64)opA <= (i64)opB);
			continue;
		case BC::I64LesserEqualU:
			opB = popU64();
			opA = popU64();
			pushU32(opA <= opB);
			continue;
		case BC::I64GreaterEqualS:
			opB = popU64();
			opA = popU64();
			pushU32((i64)opA >= (i64)opB);
			continue;
		case BC::I64GreaterEqualU:
			opB = popU64();
			opA = popU64();
			pushU32(opA >= opB);
			continue;
		case BC::F32Equal:
			opB = popU32();
			opA = popU32();
			pushU32(reinterpret_cast<f32&>(opA) == reinterpret_cast<f32&>(opB));
			continue;
		case BC::F32NotEqual:
			opB = popU32();
			opA = popU32();
			pushU32(reinterpret_cast<f32&>(opA) != reinterpret_cast<f32&>(opB));
			continue;
		case BC::F32Lesser:
			opB = popU32();
			opA = popU32();
			pushU32(reinterpret_cast<f32&>(opA) < reinterpret_cast<f32&>(opB));
			continue;
		case BC::F32Greater:
			opB = popU32();
			opA = popU32();
			pushU32(reinterpret_cast<f32&>(opA) > reinterpret_cast<f32&>(opB));
			continue;
		case BC::F32LesserEqual:
			opB = popU32();
			opA = popU32();
			pushU32(reinterpret_cast<f32&>(opA) <= reinterpret_cast<f32&>(opB));
			continue;
		case BC::F32GreaterEqual:
			opB = popU32();
			opA = popU32();
			pushU32(reinterpret_cast<f32&>(opA) >= reinterpret_cast<f32&>(opB));
			continue;
		case BC::F64Equal:
			opB = popU64();
			opA = popU64();
			pushU32(reinterpret_cast<f64&>(opA) == reinterpret_cast<f64&>(opB));
			continue;
		case BC::F64NotEqual:
			opB = popU64();
			opA = popU64();
			pushU32(reinterpret_cast<f64&>(opA) != reinterpret_cast<f64&>(opB));
			continue;
		case BC::F64Lesser:
			opB = popU64();
			opA = popU64();
			pushU32(reinterpret_cast<f64&>(opA) < reinterpret_cast<f64&>(opB));
			continue;
		case BC::F64Greater:
			opB = popU64();
			opA = popU64();
			pushU32(reinterpret_cast<f64&>(opA) > reinterpret_cast<f64&>(opB));
			continue;
		case BC::F64LesserEqual:
			opB = popU64();
			opA = popU64();
			pushU32(reinterpret_cast<f64&>(opA) <= reinterpret_cast<f64&>(opB));
			continue;
		case BC::F64GreaterEqual:
			opB = popU64();
			opA = popU64();
			pushU32(reinterpret_cast<f64&>(opA) >= reinterpret_cast<f64&>(opB));
			continue;
		case BC::I32CountLeadingZeros:
			opA = popU32();
			pushU32(std::countl_zero((u32)opA));
			continue;
		case BC::I32CountTrailingZeros:
			opA = popU32();
			pushU32(std::countr_zero((u32)opA));
			continue;
		case BC::I32CountOnes:
			opA = popU32();
			pushU32(std::popcount((u32)opA));
			continue;
		case BC::I32Add:
			opB = popU32();
			opA = popU32();
			pushU32(opA+ opB);
			continue;
		case BC::I32Subtract:
			opB = popU32();
			opA = popU32();
			pushU32(opA - opB);
			continue;
		case BC::I32Multiply:
			opB = popU32();
			opA = popU32();
			pushU32(opA * opB);
			continue;
		case BC::I32DivideS:
			opB = popU32();
			opA = popU32();
			pushU32((i32)opA / (i32)opB);
			continue;
		case BC::I32DivideU:
			opB = popU32();
			opA = popU32();
			pushU32(opA / opB);
			continue;
		case BC::I32RemainderS:
			opB = popU32();
			opA = popU32();
			pushU32((i32)opA % (i32)opB);
			continue;
		case BC::I32RemainderU:
			opB = popU32();
			opA = popU32();
			pushU32(opA % opB);
			continue;
		case BC::I32And:
			opB = popU32();
			opA = popU32();
			pushU32(opA & opB);
			continue;
		case BC::I32Or:
			opB = popU32();
			opA = popU32();
			pushU32(opA | opB);
			continue;
		case BC::I32Xor:
			opB = popU32();
			opA = popU32();
			pushU32(opA ^ opB);
			continue;
		case BC::I32ShiftLeft:
			opB = popU32();
			opA = popU32();
			pushU32(opA << opB);
			continue;
		case BC::I32ShiftRightS:
			opB = popU32();
			opA = popU32();
			pushU32((i32)opA >> (i32)opB);
			continue;
		case BC::I32ShiftRightU:
			opB = popU32();
			opA = popU32();
			pushU32(opA >> opB);
			continue;
		case BC::I32RotateLeft:
		case BC::I32RotateRight:
			break;
		case BC::I64CountLeadingZeros:
			opA = popU64();
			pushU64(std::countl_zero((u64)opA));
			continue;
		case BC::I64CountTrailingZeros:
			opA = popU64();
			pushU64(std::countr_zero((u64)opA));
			continue;
		case BC::I64CountOnes:
			opA = popU64();
			pushU64(std::popcount((u64)opA));
			continue;
		case BC::I64Add:
			opB = popU64();
			opA = popU64();
			pushU64(opA + opB);
			continue;
		case BC::I64Subtract:
		case BC::I64Multiply:
		case BC::I64DivideS:
		case BC::I64DivideU:
		case BC::I64RemainderS:
		case BC::I64RemainderU:
		case BC::I64And:
		case BC::I64Or:
		case BC::I64Xor:
		case BC::I64ShiftLeft:
		case BC::I64ShiftRightS:
		case BC::I64ShiftRightU:
		case BC::I64RotateLeft:
		case BC::I64RotateRight:
		case BC::F32Absolute:
		case BC::F32Negate:
		case BC::F32Ceil:
		case BC::F32Floor:
		case BC::F32Truncate:
		case BC::F32Nearest:
		case BC::F32SquareRoot:
		case BC::F32Add:
		case BC::F32Subtract:
		case BC::F32Multiply:
		case BC::F32Divide:
		case BC::F32Minimum:
		case BC::F32Maximum:
		case BC::F32CopySign:
		case BC::F64Absolute:
		case BC::F64Negate:
		case BC::F64Ceil:
		case BC::F64Floor:
		case BC::F64Truncate:
		case BC::F64Nearest:
		case BC::F64SquareRoot:
		case BC::F64Add:
		case BC::F64Subtract:
		case BC::F64Multiply:
		case BC::F64Divide:
		case BC::F64Minimum:
		case BC::F64Maximum:
		case BC::F64CopySign:
			break;
		case BC::I32WrapI64:
			opA = popU64();
			pushU32((u32)opA);
			continue;
		case BC::I32TruncateF32S:
			opA = popU32();
			pushU32((i32)(reinterpret_cast<f32&>(opA)));
			continue;
		case BC::I32TruncateF32U:
			opA = popU32();
			pushU32((u32)(reinterpret_cast<f32&>(opA)));
			continue;
		case BC::I32TruncateF64S:
			opA = popU64();
			pushU32((i32)(reinterpret_cast<f64&>(opA)));
			continue;
		case BC::I32TruncateF64U:
			opA = popU64();
			pushU32((u32)(reinterpret_cast<f64&>(opA)));
			continue;
		case BC::I64ExtendI32S:
			opA = popU32();
			pushU64( (i64)((i32)opA) );
			continue;
		case BC::I64ExtendI32U:
			opA = popU32();
			pushU64(opA);
			continue;
		case BC::I64TruncateF32S:
			opA = popU32();
			pushU64((i64)(reinterpret_cast<f32&>(opA)));
			continue;
		case BC::I64TruncateF32U:
			opA = popU32();
			pushU64((u64)(reinterpret_cast<f32&>(opA)));
			continue;
		case BC::I64TruncateF64S:
			opA = popU64();
			pushU64((i64)(reinterpret_cast<f64&>(opA)));
			continue;
		case BC::I64TruncateF64U:
			opA = popU64();
			pushU64((u64)(reinterpret_cast<f64&>(opA)));
			continue;
		case BC::F32ConvertI32S: {
			f32 converted = (i32)popU32();
			pushU32(reinterpret_cast<u32&>(converted));
			continue;
		}
		case BC::F32ConvertI32U: {
			f32 converted = (u32)popU32();
			pushU32(reinterpret_cast<u32&>(converted));
			continue;
		}
		case BC::F32ConvertI64S: {
			f32 converted = (i64)popU64();
			pushU32(reinterpret_cast<u32&>(converted));
			continue;
		}
		case BC::F32ConvertI64U: {
			f32 converted = (u64)popU64();
			pushU32(reinterpret_cast<u32&>(converted));
			continue;
		}
		case BC::F32DemoteF64: {
			opA = popU64();
			f32 demoted = reinterpret_cast<f64&>(opA);
			pushU32(reinterpret_cast<u32&>(demoted));
			continue;
		}
		case BC::F64ConvertI32S: {
			f64 converted = (i32)popU32();
			pushU64(reinterpret_cast<u64&>(converted));
			continue;
		}
		case BC::F64ConvertI32U: {
			f64 converted = (u32)popU32();
			pushU64(reinterpret_cast<u64&>(converted));
			continue;
		}
		case BC::F64ConvertI64S: {
			f64 converted = (i64)popU64();
			pushU64(reinterpret_cast<u64&>(converted));
			continue;
		}
		case BC::F64ConvertI64U: {
			f64 converted = (u64)popU64();
			pushU64(reinterpret_cast<u64&>(converted));
			continue;
		}
		case BC::F64PromoteF32: {
			opA = popU32();
			f64 promoted = reinterpret_cast<f32&>(opA);
			pushU64(reinterpret_cast<u64&>(promoted));
			continue;
		}
		case BC::I32Extend8s:
			opA = popU32();
			pushU32((i32)((i8)opA));
			continue;
		case BC::I32Extend16s:
			opA = popU32();
			pushU32((i32)((i16)opA));
			continue;
		case BC::I64Extend8s:
			opA = popU64();
			pushU64((i64)((i8)opA));
			continue;
		case BC::I64Extend16s:
			opA = popU64();
			pushU64((i64)((i16)opA));
			continue;
		case BC::I64Extend32s:
			opA = popU64();
			pushU64((i64)((i32)opA));
			continue;
		case BC::I32TruncateSaturateF32S:
		case BC::I32TruncateSaturateF32U:
		case BC::I32TruncateSaturateF64S:
		case BC::I32TruncateSaturateF64U:
		case BC::I64TruncateSaturateF32S:
		case BC::I64TruncateSaturateF32U:
		case BC::I64TruncateSaturateF64S:
		case BC::I64TruncateSaturateF64U:
		default:
			break;
		}

		std::cerr << "Bytecode not implemeted '" << Bytecode::fromInt(bytecode).name() << "'" << std::endl;
		throw std::runtime_error{ "bytecode not implemented" };
	}

	isInterpreting = false;
}

void ValuePack::print(std::ostream& out) const
{
	std::span<const ValType> types;
	if (isResult) {
		out << "Function result: ";
		types = functionType.results();
	}
	else {
		out << "Function parameters: ";
		types = functionType.parameters();
	}

	out << "(" << types.size() << " entries)" << std::endl << std::hex;

	u32 slotIdx = 0;
	for (auto& valType : types) {
		out << "  - ";
		if (valType.sizeInBytes() == 4) {
			out << valType.name() << " " << stackSlice[slotIdx] << std::endl;
			slotIdx++;
		}
		else {
			out << valType.name() << " " << *reinterpret_cast<u64*>(stackSlice.data()+ slotIdx) << std::endl;
			slotIdx += 2;
		}
	}

	out << std::dec;
}
