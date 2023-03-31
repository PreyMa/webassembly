

#include <iostream>
#include <iomanip>
#include <cassert>

#include "interpreter.h"
#include "introspection.h"
#include "bytecode.h"

using namespace WASM;

HostFunctionBase::HostFunctionBase(FunctionType ft)
	: mFunctionType{ std::move(ft) } {}

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

	auto introspector= Nullable<Introspector>::fromPointer(attachedIntrospector);

	auto buffer= Buffer::fromFile(path);
	ModuleParser parser{ introspector };
	parser.parse(std::move(buffer), std::move(path));

	ModuleValidator validator{ introspector };
	validator.validate(parser);

	modules.emplace_back( parser.toModule() );
	auto& module= modules.back();

	auto result= moduleNameMap.emplace( module.name(), module );
	if (!result.second) {
		throw std::runtime_error{ "Module name collision" };
	}
}

void Interpreter::compileAndLinkModules()
{
	if (hasLinked) {
		throw std::runtime_error{ "Already linked" };
	}

	{
		ModuleLinker linker{ *this, modules };
		linker.link();
	}

	for (auto& module : modules) {
		auto introspector = Nullable<Introspector>::fromPointer(attachedIntrospector);
		module.initTables(introspector);
		module.initGlobals(introspector);

		ModuleCompiler compiler{ *this, module };
		compiler.compile();
	}

	hasLinked = true;
}

void Interpreter::runStartFunctions()
{
	for (auto& module : modules) {
		auto startFunction = module.startFunction();
		if (startFunction.has_value()) {
			executeFunction(*startFunction, {});
		}
	}
}

void Interpreter::attachIntrospector(std::unique_ptr<Introspector> introspector)
{
	attachedIntrospector = std::move(introspector);
}

ValuePack Interpreter::executeFunction(Function& function, std::span<Value> values)
{
	assert(!isInterpreting);

	auto bytecodeFunction = function.asBytecodeFunction();
	if (bytecodeFunction.has_value()) {
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

u32 Interpreter::indexOfDeduplicatedFunctionType(FunctionType& funcType) const
{
	auto beginIt = functionTypes.begin();
	auto findIt = std::find(beginIt, functionTypes.end(), funcType);
	assert(findIt != functionTypes.end());
	return findIt - beginIt;
}

ValuePack Interpreter::runInterpreterLoop(const BytecodeFunction& function, std::span<Value> parameters)
{
	assert(!isInterpreting);
	isInterpreting = true;

	// FIXME: Do not hard code the stack size
	stackBase = std::make_unique<u32[]>(4096);

	const u8* instructionPointer = function.bytecode().begin();
	u32* stackPointer = stackBase.get();
	u32* framePointer = stackBase.get();
	Module* modulePointer = nullptr;

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

	// Push frame data to stace
	pushPtr(0x00);
	pushPtr(framePointer);
	pushPtr(stackBase.get());
	pushPtr(modulePointer);

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
			break;
		case BC::ReturnFew: {
			auto numSlotsToReturn = *(instructionPointer++);
			auto currentStackPointer = stackPointer;
			instructionPointer = (u8*)loadPtrWithFrameOffset(0);
			auto oldFramePointer = (u32*)loadPtrWithFrameOffset(1);
			stackPointer = (u32*)loadPtrWithFrameOffset(2);
			modulePointer = (Module*)loadPtrWithFrameOffset(3);
			framePointer = oldFramePointer;

			for (i64 i = 0; i != numSlotsToReturn; i++) {
				pushU32(currentStackPointer[i-numSlotsToReturn]);
			}

			if (!instructionPointer) {
				std::cout << "Execution finished" << std::endl;
				return ValuePack{ function.functionType(), true, {stackBase.get(), (sizeType)(stackPointer - stackBase.get())} };
			}
			continue;
		}
		case BC::ReturnMany:
			break;
		case BC::Call: {
			auto callee = (BytecodeFunction*)loadOperandPtr();
			auto stackParameterSection = loadOperandU32();
			auto stackPointerToSave = stackPointer - stackParameterSection;
			auto newFramePointer = stackPointer;

			if (callee->maxStackHeight() + stackPointer > stackBase.get() + 4069) {
				throw std::runtime_error{ "Stack overflow" };
			}

			pushPtr(instructionPointer);
			pushPtr(framePointer);
			pushPtr(stackPointerToSave);
			pushPtr(modulePointer);

			// FIXME: This value should not need to be calculated
			for (u32 i = 0; i != callee->localsSizeInBytes() / 4; i++) {
				pushU32(0);
			}

			framePointer = newFramePointer;
			instructionPointer = callee->bytecode().begin();
			modulePointer = nullptr;
			continue;
		}
		case BC::CallIndirect:
		case BC::CallHost:
			break;
		case BC::Entry: {
			modulePointer = (Module*)loadOperandPtr();
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
		case BC::I64Select:
			break;
		case BC::I32LocalGetFar:
			opA = loadOperandU32();
			pushU32(stackPointer[-opA]);
			continue;
		case BC::I32LocalSetFar:
		case BC::I32LocalTeeFar:
			break;
		case BC::I32LocalGetNear:
			opA = *(instructionPointer++);
			pushU32(stackPointer[-opA]);
			continue;
		case BC::I32LocalSetNear:
			opA = *(instructionPointer++);
			stackPointer[-opA]= popU32();
			continue;
		case BC::I32LocalTeeNear:
		case BC::I64LocalGetFar:
		case BC::I64LocalSetFar:
		case BC::I64LocalTeeFar:
		case BC::I64LocalGetNear:
		case BC::I64LocalSetNear:
		case BC::I64LocalTeeNear:
			break;
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
			assert(modulePointer);
			opC = *(instructionPointer++);
			opB = popU32();
			opA = popU32();
			*reinterpret_cast<u32*>(modulePointer->memoryWithIndexZero()->pointer(opC + opA)) = opB;
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
			assert(modulePointer);
			pushU32(modulePointer->memoryWithIndexZero()->currentSize());
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
		case BC::I32Equal:
		case BC::I32NotEqual:
			break;
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
		case BC::I32GreaterU:opB = popU32();
			opA = popU32();
			pushU32(opA > opB);
			continue;
		case BC::I32LesserEqualS:
		case BC::I32LesserEqualU:
		case BC::I32GreaterEqualS:
		case BC::I32GreaterEqualU:
		case BC::I64EqualZero:
		case BC::I64Equal:
		case BC::I64NotEqual:
		case BC::I64LesserS:
		case BC::I64LesserU:
		case BC::I64GreaterS:
		case BC::I64GreaterU:
		case BC::I64LesserEqualS:
		case BC::I64LesserEqualU:
		case BC::I64GreaterEqualS:
		case BC::I64GreaterEqualU:
		case BC::F32Equal:
		case BC::F32NotEqual:
		case BC::F32Lesser:
		case BC::F32Greater:
		case BC::F32LesserEqual:
		case BC::F32GreaterEqual:
		case BC::F64Equal:
		case BC::F64NotEqual:
		case BC::F64Lesser:
		case BC::F64Greater:
		case BC::F64LesserEqual:
		case BC::F64GreaterEqual:
		case BC::I32CountLeadingZeros:
		case BC::I32CountTrailingZeros:
		case BC::I32CountOnes:
			break;
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
		case BC::I64CountLeadingZeros:
		case BC::I64CountTrailingZeros:
		case BC::I64CountOnes:
		case BC::I64Add:
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
		case BC::I32WrapI64:
		case BC::I32TruncateF32S:
		case BC::I32TruncateF32U:
		case BC::I32TruncateF64S:
		case BC::I32TruncateF64U:
		case BC::I64ExtendI32S:
		case BC::I64ExtendI32U:
		case BC::I64TruncateF32S:
		case BC::I64TruncateF32U:
		case BC::I64TruncateF64S:
		case BC::I64TruncateF64U:
		case BC::F32ConvertI32S:
		case BC::F32ConvertI32U:
		case BC::F32ConvertI64S:
		case BC::F32ConvertI64U:
		case BC::F32DemoteF64:
		case BC::F64ConvertI32S:
		case BC::F64ConvertI32U:
		case BC::F64ConvertI64S:
		case BC::F64ConvertI64U:
		case BC::F64PromoteF32:
		case BC::I32Extend8s:
		case BC::I32Extend16s:
		case BC::I64Extend8s:
		case BC::I64Extend16s:
		case BC::I64Extend32s:
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
