#include <stdexcept>
#include <cassert>
#include <ostream>

#include "module.h"
#include "bytecode.h"
#include "error.h"

using namespace WASM;

static constexpr bool isPowerOfTwo(u64 x) {
	return (x & (x - 1)) == 0;
}

InstructionType InstructionType::fromWASMBytes(BufferIterator& it)
{
	auto byte = it.nextU8();
	switch (byte) {
		case 0x00: return Unreachable;
		case 0x01: return NoOperation;
		case 0x02: return Block;
		case 0x03: return Loop;
		case 0x04: return If;
		case 0x05: return Else;
		case 0x0B: return End;
		case 0x0C: return Branch;
		case 0x0D: return BranchIf;
		case 0x0E: return BranchTable;
		case 0x0F: return Return;
		case 0x10: return Call;
		case 0x11: return CallIndirect;
		case 0x1A: return Drop;
		case 0x1B: return Select;
		case 0x1C: return SelectFrom;
		case 0x20: return LocalGet;
		case 0x21: return LocalSet;
		case 0x22: return LocalTee;
		case 0x23: return GlobalGet;
		case 0x24: return GlobalSet;
		case 0x25: return TableGet;
		case 0x26: return TableSet;
		case 0x28: return I32Load;
		case 0x29: return I64Load;
		case 0x2A: return F32Load;
		case 0x2B: return F64Load;
		case 0x2C: return I32Load8s;
		case 0x2D: return I32Load8u;
		case 0x2E: return I32Load16s;
		case 0x2F: return I32Load16u;
		case 0x30: return I64Load8s;
		case 0x31: return I64Load8u;
		case 0x32: return I64Load16s;
		case 0x33: return I64Load16u;
		case 0x34: return I64Load32s;
		case 0x35: return I64Load32u;
		case 0x36: return I32Store;
		case 0x37: return I64Store;
		case 0x38: return F32Store;
		case 0x39: return F64Store;
		case 0x3A: return I32Store8;
		case 0x3B: return I32Store16;
		case 0x3C: return I64Store8;
		case 0x3D: return I64Store16;
		case 0x3E: return I64Store32;
		case 0x3F: return MemorySize;
		case 0x40: return MemoryGrow;
		case 0x41: return I32Const;
		case 0x42: return I64Const;
		case 0x43: return F32Const;
		case 0x44: return F64Const;
		case 0x45: return I32EqualZero;
		case 0x46: return I32Equal;
		case 0x47: return I32NotEqual;
		case 0x48: return I32LesserS;
		case 0x49: return I32LesserU;
		case 0x4A: return I32GreaterS;
		case 0x4B: return I32GreaterU;
		case 0x4C: return I32LesserEqualS;
		case 0x4D: return I32LesserEqualU;
		case 0x4E: return I32GreaterEqualS;
		case 0x4F: return I32GreaterEqualU;
		case 0x50: return I64EqualZero;
		case 0x51: return I64Equal;
		case 0x52: return I64NotEqual;
		case 0x53: return I64LesserS;
		case 0x54: return I64LesserU;
		case 0x55: return I64GreaterS;
		case 0x56: return I64GreaterU;
		case 0x57: return I64LesserEqualS;
		case 0x58: return I64LesserEqualU;
		case 0x59: return I64GreaterEqualS;
		case 0x5A: return I64GreaterEqualU;
		case 0x5B: return F32Equal;
		case 0x5C: return F32NotEqual;
		case 0x5D: return F32Lesser;
		case 0x5E: return F32Greater;
		case 0x5F: return F32LesserEqual;
		case 0x60: return F32GreaterEqual;
		case 0x61: return F64Equal;
		case 0x62: return F64NotEqual;
		case 0x63: return F64Lesser;
		case 0x64: return F64Greater;
		case 0x65: return F64LesserEqual;
		case 0x66: return F64GreaterEqual;
		case 0x67: return I32CountLeadingZeros;
		case 0x68: return I32CountTrailingZeros;
		case 0x69: return I32CountOnes;
		case 0x6A: return I32Add;
		case 0x6B: return I32Subtract;
		case 0x6C: return I32Multiply;
		case 0x6D: return I32DivideS;
		case 0x6E: return I32DivideU;
		case 0x6F: return I32RemainderS;
		case 0x70: return I32RemainderU;
		case 0x71: return I32And;
		case 0x72: return I32Or;
		case 0x73: return I32Xor;
		case 0x74: return I32ShiftLeft;
		case 0x75: return I32ShiftRightS;
		case 0x76: return I32ShiftRightU;
		case 0x77: return I32RotateLeft;
		case 0x78: return I32RotateRight;
		case 0x79: return I64CountLeadingZeros;
		case 0x7A: return I64CountTrailingZeros;
		case 0x7B: return I64CountOnes;
		case 0x7C: return I64Add;
		case 0x7D: return I64Subtract;
		case 0x7E: return I64Multiply;
		case 0x7F: return I64DivideS;
		case 0x80: return I64DivideU;
		case 0x81: return I64RemainderS;
		case 0x82: return I64RemainderU;
		case 0x83: return I64And;
		case 0x84: return I64Or;
		case 0x85: return I64Xor;
		case 0x86: return I64ShiftLeft;
		case 0x87: return I64ShiftRightS;
		case 0x88: return I64ShiftRightU;
		case 0x89: return I64RotateLeft;
		case 0x8A: return I64RotateRight;
		case 0x8B: return F32Absolute;
		case 0x8C: return F32Negate;
		case 0x8D: return F32Ceil;
		case 0x8E: return F32Floor;
		case 0x8F: return F32Truncate;
		case 0x90: return F32Nearest;
		case 0x91: return F32SquareRoot;
		case 0x92: return F32Add;
		case 0x93: return F32Subtract;
		case 0x94: return F32Multiply;
		case 0x95: return F32Divide;
		case 0x96: return F32Minimum;
		case 0x97: return F32Maximum;
		case 0x98: return F32CopySign;
		case 0x99: return F64Absolute;
		case 0x9A: return F64Negate;
		case 0x9B: return F64Ceil;
		case 0x9C: return F64Floor;
		case 0x9D: return F64Truncate;
		case 0x9E: return F64Nearest;
		case 0x9F: return F64SquareRoot;
		case 0xA0: return F64Add;
		case 0xA1: return F64Subtract;
		case 0xA2: return F64Multiply;
		case 0xA3: return F64Divide;
		case 0xA4: return F64Minimum;
		case 0xA5: return F64Maximum;
		case 0xA6: return F64CopySign;
		case 0xA7: return I32WrapI64;
		case 0xA8: return I32TruncateF32S;
		case 0xA9: return I32TruncateF32U;
		case 0xAA: return I32TruncateF64S;
		case 0xAB: return I32TruncateF64U;
		case 0xAC: return I64ExtendI32S;
		case 0xAD: return I64ExtendI32U;
		case 0xAE: return I64TruncateF32S;
		case 0xAF: return I64TruncateF32U;
		case 0xB0: return I64TruncateF64S;
		case 0xB1: return I64TruncateF64U;
		case 0xB2: return F32ConvertI32S;
		case 0xB3: return F32ConvertI32U;
		case 0xB4: return F32ConvertI64S;
		case 0xB5: return F32ConvertI64U;
		case 0xB6: return F32DemoteF64;
		case 0xB7: return F64ConvertI32S;
		case 0xB8: return F64ConvertI32U;
		case 0xB9: return F64ConvertI64S;
		case 0xBA: return F64ConvertI64U;
		case 0xBB: return F64PromoteF32;
		case 0xBC: return I32ReinterpretF32;
		case 0xBD: return I64ReinterpretF64;
		case 0xBE: return F32ReinterpretI32;
		case 0xBF: return F64ReinterpretI64;
		case 0xC0: return I32Extend8s;
		case 0xC1: return I32Extend16s;
		case 0xC2: return I64Extend8s;
		case 0xC3: return I64Extend16s;
		case 0xC4: return I64Extend32s;
		case 0xD0: return ReferenceNull;
		case 0xD1: return ReferenceIsNull;
		case 0xD2: return ReferenceFunction;
		case 0xFC: {
			auto extension = it.nextU32();
			switch (extension) {
			case 0: return I32TruncateSaturateF32S;
			case 1: return I32TruncateSaturateF32U;
			case 2: return I32TruncateSaturateF64S;
			case 3: return I32TruncateSaturateF64U;
			case 4: return I64TruncateSaturateF32S;
			case 5: return I64TruncateSaturateF32U;
			case 6: return I64TruncateSaturateF64S;
			case 7: return I64TruncateSaturateF64U;
			case 8: return MemoryInit;
			case 9: return DataDrop;
			case 10: return MemoryCopy;
			case 11: return MemoryFill;
			case 12: return TableInit;
			case 13: return ElementDrop;
			case 14: return TableCopy;
			case 15: return TableGrow;
			case 16: return TableSize;
			case 17: return TableFill;
			default:
				throw std::runtime_error{ "Unknown secondary instruction byte code." };
			}
		}
		case 0xFD:
			throw std::runtime_error{ "Vector instructions are not supported." };
		
		default:
			throw std::runtime_error{ "Unknown instruction byte code." };
	}
}

const char* InstructionType::name() const
{
	switch (value) {
		case Unreachable: return "Unreachable";
		case NoOperation: return "NoOperation";
		case Block: return "Block";
		case Loop: return "Loop";
		case If: return "If";
		case Else: return "Else";
		case End: return "End";
		case Branch: return "Branch";
		case BranchIf: return "BranchIf";
		case BranchTable: return "BranchTable";
		case Return: return "Return";
		case Call: return "Call";
		case CallIndirect: return "CallIndirect";
		case Drop: return "Drop";
		case Select: return "Select";
		case SelectFrom: return "SelectFrom";
		case LocalGet: return "LocalGet";
		case LocalSet: return "LocalSet";
		case LocalTee: return "LocalTee";
		case GlobalGet: return "GlobalGet";
		case GlobalSet: return "GlobalSet";
		case ReferenceNull: return "ReferenceNull";
		case ReferenceIsNull: return "ReferenceIsNull";
		case ReferenceFunction: return "ReferenceFunction";
		case TableGet: return "TableGet";
		case TableSet: return "TableSet";
		case TableInit: return "TableInit";
		case ElementDrop: return "ElementDrop";
		case TableCopy: return "TableCopy";
		case TableGrow: return "TableGrow";
		case TableSize: return "TableSize";
		case TableFill: return "TableFill";
		case I32Load: return "I32Load";
		case I64Load: return "I64Load";
		case F32Load: return "F32Load";
		case F64Load: return "F64Load";
		case I32Load8s: return "I32Load8s";
		case I32Load8u: return "I32Load8u";
		case I32Load16s: return "I32Load16s";
		case I32Load16u: return "I32Load16u";
		case I64Load8s: return "I64Load8s";
		case I64Load8u: return "I64Load8u";
		case I64Load16s: return "I64Load16s";
		case I64Load16u: return "I64Load16u";
		case I64Load32s: return "I64Load32s";
		case I64Load32u: return "I64Load32u";
		case I32Store: return "I32Store";
		case I64Store: return "I64Store";
		case F32Store: return "F32Store";
		case F64Store: return "F64Store";
		case I32Store8: return "I32Store8";
		case I32Store16: return "I32Store16";
		case I64Store8: return "I64Store8";
		case I64Store16: return "I64Store16";
		case I64Store32: return "I64Store32";
		case MemorySize: return "MemorySize";
		case MemoryGrow: return "MemoryGrow";
		case MemoryInit: return "MemoryInit";
		case DataDrop: return "DataDrop";
		case MemoryCopy: return "MemoryCopy";
		case MemoryFill: return "MemoryFill";
		case I32Const: return "I32Const";
		case I64Const: return "I64Const";
		case F32Const: return "F32Const";
		case F64Const: return "F64Const";
		case I32EqualZero: return "I32EqualZero";
		case I32Equal: return "I32Equal";
		case I32NotEqual: return "I32NotEqual";
		case I32LesserS: return "I32LesserS";
		case I32LesserU: return "I32LesserU";
		case I32GreaterS: return "I32GreaterS";
		case I32GreaterU: return "I32GreaterU";
		case I32LesserEqualS: return "I32LesserEqualS";
		case I32LesserEqualU: return "I32LesserEqualU";
		case I32GreaterEqualS: return "I32GreaterEqualS";
		case I32GreaterEqualU: return "I32GreaterEqualU";
		case I64EqualZero: return "I64EqualZero";
		case I64Equal: return "I64Equal";
		case I64NotEqual: return "I64NotEqual";
		case I64LesserS: return "I64LesserS";
		case I64LesserU: return "I64LesserU";
		case I64GreaterS: return "I64GreaterS";
		case I64GreaterU: return "I64GreaterU";
		case I64LesserEqualS: return "I64LesserEqualS";
		case I64LesserEqualU: return "I64LesserEqualU";
		case I64GreaterEqualS: return "I64GreaterEqualS";
		case I64GreaterEqualU: return "I64GreaterEqualU";
		case F32Equal: return "F32Equal";
		case F32NotEqual: return "F32NotEqual";
		case F32Lesser: return "F32Lesser";
		case F32Greater: return "F32Greater";
		case F32LesserEqual: return "F32LesserEqual";
		case F32GreaterEqual: return "F32GreaterEqual";
		case F64Equal: return "F64Equal";
		case F64NotEqual: return "F64NotEqual";
		case F64Lesser: return "F64Lesser";
		case F64Greater: return "F64Greater";
		case F64LesserEqual: return "F64LesserEqual";
		case F64GreaterEqual: return "F64GreaterEqual";
		case I32CountLeadingZeros: return "I32CountLeadingZeros";
		case I32CountTrailingZeros: return "I32CountTrailingZeros";
		case I32CountOnes: return "I32CountOnes";
		case I32Add: return "I32Add";
		case I32Subtract: return "I32Subtract";
		case I32Multiply: return "I32Multiply";
		case I32DivideS: return "I32DivideS";
		case I32DivideU: return "I32DivideU";
		case I32RemainderS: return "I32RemainderS";
		case I32RemainderU: return "I32RemainderU";
		case I32And: return "I32And";
		case I32Or: return "I32Or";
		case I32Xor: return "I32Xor";
		case I32ShiftLeft: return "I32ShiftLeft";
		case I32ShiftRightS: return "I32ShiftRightS";
		case I32ShiftRightU: return "I32ShiftRightU";
		case I32RotateLeft: return "I32RotateLeft";
		case I32RotateRight: return "I32RotateRight";
		case I64CountLeadingZeros: return "I64CountLeadingZeros";
		case I64CountTrailingZeros: return "I64CountTrailingZeros";
		case I64CountOnes: return "I64CountOnes";
		case I64Add: return "I64Add";
		case I64Subtract: return "I64Subtract";
		case I64Multiply: return "I64Multiply";
		case I64DivideS: return "I64DivideS";
		case I64DivideU: return "I64DivideU";
		case I64RemainderS: return "I64RemainderS";
		case I64RemainderU: return "I64RemainderU";
		case I64And: return "I64And";
		case I64Or: return "I64Or";
		case I64Xor: return "I64Xor";
		case I64ShiftLeft: return "I64ShiftLeft";
		case I64ShiftRightS: return "I64ShiftRightS";
		case I64ShiftRightU: return "I64ShiftRightU";
		case I64RotateLeft: return "I64RotateLeft";
		case I64RotateRight: return "I64RotateRight";
		case F32Absolute: return "F32Absolute";
		case F32Negate: return "F32Negate";
		case F32Ceil: return "F32Ceil";
		case F32Floor: return "F32Floor";
		case F32Truncate: return "F32Truncate";
		case F32Nearest: return "F32Nearest";
		case F32SquareRoot: return "F32SquareRoot";
		case F32Add: return "F32Add";
		case F32Subtract: return "F32Subtract";
		case F32Multiply: return "F32Multiply";
		case F32Divide: return "F32Divide";
		case F32Minimum: return "F32Minimum";
		case F32Maximum: return "F32Maximum";
		case F32CopySign: return "F32CopySign";
		case F64Absolute: return "F64Absolute";
		case F64Negate: return "F64Negate";
		case F64Ceil: return "F64Ceil";
		case F64Floor: return "F64Floor";
		case F64Truncate: return "F64Truncate";
		case F64Nearest: return "F64Nearest";
		case F64SquareRoot: return "F64SquareRoot";
		case F64Add: return "F64Add";
		case F64Subtract: return "F64Subtract";
		case F64Multiply: return "F64Multiply";
		case F64Divide: return "F64Divide";
		case F64Minimum: return "F64Minimum";
		case F64Maximum: return "F64Maximum";
		case F64CopySign: return "F64CopySign";
		case I32WrapI64: return "I32WrapI64";
		case I32TruncateF32S: return "I32TruncateF32S";
		case I32TruncateF32U: return "I32TruncateF32U";
		case I32TruncateF64S: return "I32TruncateF64S";
		case I32TruncateF64U: return "I32TruncateF64U";
		case I64ExtendI32S: return "I64ExtendI32S";
		case I64ExtendI32U: return "I64ExtendI32U";
		case I64TruncateF32S: return "I64TruncateF32S";
		case I64TruncateF32U: return "I64TruncateF32U";
		case I64TruncateF64S: return "I64TruncateF64S";
		case I64TruncateF64U: return "I64TruncateF64U";
		case F32ConvertI32S: return "F32ConvertI32S";
		case F32ConvertI32U: return "F32ConvertI32U";
		case F32ConvertI64S: return "F32ConvertI64S";
		case F32ConvertI64U: return "F32ConvertI64U";
		case F32DemoteF64: return "F32DemoteF64";
		case F64ConvertI32S: return "F64ConvertI32S";
		case F64ConvertI32U: return "F64ConvertI32U";
		case F64ConvertI64S: return "F64ConvertI64S";
		case F64ConvertI64U: return "F64ConvertI64U";
		case F64PromoteF32: return "F64PromoteF32";
		case I32ReinterpretF32: return "I32ReinterpretF32";
		case I64ReinterpretF64: return "I64ReinterpretF64";
		case F32ReinterpretI32: return "F32ReinterpretI32";
		case F64ReinterpretI64: return "F64ReinterpretI64";
		case I32Extend8s: return "I32Extend8s";
		case I32Extend16s: return "I32Extend16s";
		case I64Extend8s: return "I64Extend8s";
		case I64Extend16s: return "I64Extend16s";
		case I64Extend32s: return "I64Extend32s";
		case I32TruncateSaturateF32S: return "I32TruncateSaturateF32S";
		case I32TruncateSaturateF32U: return "I32TruncateSaturateF32U";
		case I32TruncateSaturateF64S: return "I32TruncateSaturateF64S";
		case I32TruncateSaturateF64U: return "I32TruncateSaturateF64U";
		case I64TruncateSaturateF32S: return "I64TruncateSaturateF32S";
		case I64TruncateSaturateF32U: return "I64TruncateSaturateF32U";
		case I64TruncateSaturateF64S: return "I64TruncateSaturateF64S";
		case I64TruncateSaturateF64U: return "I64TruncateSaturateF64U";
		default: return "<unknown instruction type>";
	}
}

Instruction Instruction::fromWASMBytes(BufferIterator& it)
{
	bool is64BitMemoryInstruction= false;
	using IT = InstructionType;
	auto type = InstructionType::fromWASMBytes(it);
	switch (type) {
	case IT::Unreachable:
	case IT::NoOperation:
		return { type };
	case IT::Block:
	case IT::Loop:
	case IT::If: 
		return parseBlockTypeInstruction(type, it);
	case IT::Else:
	case IT::End:
		return { type };
	case IT::Branch:
	case IT::BranchIf: {
		auto lableIdx = it.nextU32();
		return { type, lableIdx };
	}
	case IT::BranchTable:
		return parseBranchTableInstruction( it );
	case IT::Return:
		return { type };
	case IT::Call: {
		auto funcIdx = it.nextU32();
		return { type, funcIdx };
	}
	case IT::CallIndirect:{
		auto typeIdx = it.nextU32();
		auto tableIdx = it.nextU32();
		return { type, typeIdx, tableIdx }; 
	}
	case IT::Drop:
	case IT::Select:
		return { type };
	case IT::SelectFrom:
		return parseSelectVectorInstruction( it );
	case IT::LocalGet:
	case IT::LocalSet:
	case IT::LocalTee:
	case IT::GlobalGet:
	case IT::GlobalSet: {
		auto localIdx = it.nextU32();
		return { type, localIdx };
	}
	case IT::ReferenceNull:{
		auto refType = ValType::fromInt(it.nextU8());
		if (!refType.isReference()) {
			throw std::runtime_error{ "Expected reference type for ref.null instruction" };
		}
		return { type, (u32)refType }; 
	}
	case IT::ReferenceIsNull:
		return { type };
	case IT::ReferenceFunction: {
		auto funcIdx = it.nextU32();
		return { type, funcIdx };
	}
	case IT::TableGet:
	case IT::TableSet:
	case IT::TableGrow:
	case IT::TableSize:
	case IT::TableFill: {
		auto tableIdx = it.nextU32();
		return { type, tableIdx };
	}
	case IT::ElementDrop: {
		auto elementIdx = it.nextU32();
		return { type, (u32)0, elementIdx };
	}
	case IT::TableInit:
	case IT::TableCopy: {
		auto elementIdx = it.nextU32();
		auto tableIdx = it.nextU32();
		return { type, tableIdx, elementIdx };
	}
	case IT::I64Load:
	case IT::F64Load:
	case IT::I64Load8s:
	case IT::I64Load8u:
	case IT::I64Load16s:
	case IT::I64Load16u:
	case IT::I64Load32s:
	case IT::I64Load32u:
	case IT::I64Store:
	case IT::F64Store:
	case IT::I64Store8:
	case IT::I64Store16:
	case IT::I64Store32:
		is64BitMemoryInstruction = true;
		// Fall through
	case IT::I32Load:
	case IT::F32Load:
	case IT::I32Load8s:
	case IT::I32Load8u:
	case IT::I32Load16s:
	case IT::I32Load16u:
	case IT::I32Store:
	case IT::F32Store:
	case IT::I32Store8:
	case IT::I32Store16:{

		auto alignment = it.nextU32();
		auto offset = it.nextU32();
		auto alignmentInBytes = 0x1 << alignment;
		auto typeSizeInBytes = is64BitMemoryInstruction ? 8 : 4;
		if (alignmentInBytes > typeSizeInBytes) {
			throw std::runtime_error{ "Memory alignment has to be power of 2" };
		}
		return { type, alignment, offset }; 
	}
	case IT::MemorySize:
	case IT::MemoryGrow: {
		it.assertU8(0x00); // Only memory idx 0x00 is supported
		return { type }; 
	}
	case IT::MemoryInit: {
		auto dataIdx = it.nextU32();
		it.assertU8(0x00); // Only memory idx 0x00 is supported
		return { type, dataIdx };
	}
	case IT::DataDrop: {
		auto dataIdx = it.nextU32();
		return { type, dataIdx }; 
	}
	case IT::MemoryCopy: {
		it.assertU8(0x00); // Only memory idx 0x00 is supported
		it.assertU8(0x00);
		return { type }; 
	}
	case IT::MemoryFill: {
		it.assertU8(0x00); // Only memory idx 0x00 is supported
		return { type }; 
	}
	case IT::I32Const: {
		auto constant = it.nextI32();
		return { type, Constant{ constant } };
	}
	case IT::I64Const: {
		auto constant = it.nextI64();
		return { type, Constant{ constant } };
	}
	case IT::F32Const: {
		auto constant = it.nextF32();
		return { type, Constant{ constant } };
	}
	case IT::F64Const: {
		auto constant = it.nextF64();
		return { type, Constant{ constant } };
	}
	case IT::I32EqualZero:
	case IT::I32Equal:
	case IT::I32NotEqual:
	case IT::I32LesserS:
	case IT::I32LesserU:
	case IT::I32GreaterS:
	case IT::I32GreaterU:
	case IT::I32LesserEqualS:
	case IT::I32LesserEqualU:
	case IT::I32GreaterEqualS:
	case IT::I32GreaterEqualU:
	case IT::I64EqualZero:
	case IT::I64Equal:
	case IT::I64NotEqual:
	case IT::I64LesserS:
	case IT::I64LesserU:
	case IT::I64GreaterS:
	case IT::I64GreaterU:
	case IT::I64LesserEqualS:
	case IT::I64LesserEqualU:
	case IT::I64GreaterEqualS:
	case IT::I64GreaterEqualU:
	case IT::F32Equal:
	case IT::F32NotEqual:
	case IT::F32Lesser:
	case IT::F32Greater:
	case IT::F32LesserEqual:
	case IT::F32GreaterEqual:
	case IT::F64Equal:
	case IT::F64NotEqual:
	case IT::F64Lesser:
	case IT::F64Greater:
	case IT::F64LesserEqual:
	case IT::F64GreaterEqual:
	case IT::I32CountLeadingZeros:
	case IT::I32CountTrailingZeros:
	case IT::I32CountOnes:
	case IT::I32Add:
	case IT::I32Subtract:
	case IT::I32Multiply:
	case IT::I32DivideS:
	case IT::I32DivideU:
	case IT::I32RemainderS:
	case IT::I32RemainderU:
	case IT::I32And:
	case IT::I32Or:
	case IT::I32Xor:
	case IT::I32ShiftLeft:
	case IT::I32ShiftRightS:
	case IT::I32ShiftRightU:
	case IT::I32RotateLeft:
	case IT::I32RotateRight:
	case IT::I64CountLeadingZeros:
	case IT::I64CountTrailingZeros:
	case IT::I64CountOnes:
	case IT::I64Add:
	case IT::I64Subtract:
	case IT::I64Multiply:
	case IT::I64DivideS:
	case IT::I64DivideU:
	case IT::I64RemainderS:
	case IT::I64RemainderU:
	case IT::I64And:
	case IT::I64Or:
	case IT::I64Xor:
	case IT::I64ShiftLeft:
	case IT::I64ShiftRightS:
	case IT::I64ShiftRightU:
	case IT::I64RotateLeft:
	case IT::I64RotateRight:
	case IT::F32Absolute:
	case IT::F32Negate:
	case IT::F32Ceil:
	case IT::F32Floor:
	case IT::F32Truncate:
	case IT::F32Nearest:
	case IT::F32SquareRoot:
	case IT::F32Add:
	case IT::F32Subtract:
	case IT::F32Multiply:
	case IT::F32Divide:
	case IT::F32Minimum:
	case IT::F32Maximum:
	case IT::F32CopySign:
	case IT::F64Absolute:
	case IT::F64Negate:
	case IT::F64Ceil:
	case IT::F64Floor:
	case IT::F64Truncate:
	case IT::F64Nearest:
	case IT::F64SquareRoot:
	case IT::F64Add:
	case IT::F64Subtract:
	case IT::F64Multiply:
	case IT::F64Divide:
	case IT::F64Minimum:
	case IT::F64Maximum:
	case IT::F64CopySign:
	case IT::I32WrapI64:
	case IT::I32TruncateF32S:
	case IT::I32TruncateF32U:
	case IT::I32TruncateF64S:
	case IT::I32TruncateF64U:
	case IT::I64ExtendI32S:
	case IT::I64ExtendI32U:
	case IT::I64TruncateF32S:
	case IT::I64TruncateF32U:
	case IT::I64TruncateF64S:
	case IT::I64TruncateF64U:
	case IT::F32ConvertI32S:
	case IT::F32ConvertI32U:
	case IT::F32ConvertI64S:
	case IT::F32ConvertI64U:
	case IT::F32DemoteF64:
	case IT::F64ConvertI32S:
	case IT::F64ConvertI32U:
	case IT::F64ConvertI64S:
	case IT::F64ConvertI64U:
	case IT::F64PromoteF32:
	case IT::I32ReinterpretF32:
	case IT::I64ReinterpretF64:
	case IT::F32ReinterpretI32:
	case IT::F64ReinterpretI64:
	case IT::I32Extend8s:
	case IT::I32Extend16s:
	case IT::I64Extend8s:
	case IT::I64Extend16s:
	case IT::I64Extend32s:
	case IT::I32TruncateSaturateF32S:
	case IT::I32TruncateSaturateF32U:
	case IT::I32TruncateSaturateF64S:
	case IT::I32TruncateSaturateF64U:
	case IT::I64TruncateSaturateF32S:
	case IT::I64TruncateSaturateF32U:
	case IT::I64TruncateSaturateF64S:
	case IT::I64TruncateSaturateF64U:
		return { type };
	default:
		throw std::runtime_error{ "Could not create instruction object from unkown instruction type" };
	}
}

Instruction Instruction::parseBlockTypeInstruction(InstructionType type, BufferIterator& it)
{
	auto blockType = it.peekU8();
	if (blockType == 0x40) {
		it.nextU8();
		return { type, BlockType::None };
	}

	if (blockType < ValType::NumberOfItems) {
		auto valType = ValType::fromInt(blockType);
		if (valType.isValid()) {
			it.nextU8();
			return { type, BlockType::ValType, (u32)valType };
		}
	}

	auto typeIdx = it.nextI64(); // Actually a i33
	if (typeIdx < 0) {
		throw std::runtime_error{ "Expected positive type index for block type" };
	}
	return { type, BlockType::TypeIndex, (u32)typeIdx };
}

Instruction Instruction::parseBranchTableInstruction(BufferIterator& it)
{
	// Consume all values in the vector
	auto position = it.positionPointer();
	auto numLabels = it.nextU32();
	for (u32 i = 0; i != numLabels; i++) {
		it.nextU32();
	}
	auto defaultLabel = it.nextU32();

	return { InstructionType::BranchTable, position, defaultLabel };
}

Instruction Instruction::parseSelectVectorInstruction(BufferIterator& it)
{
	// Consume all values in the vector (each valtype is a single byte)
	auto position = it.positionPointer();
	auto numTypes = it.nextU32();
	it += numTypes;

	return { InstructionType::SelectFrom, position };
}

void Instruction::printBranchTableInstruction(std::ostream& out, const BufferSlice& data) const
{
	// FIXME: If buffer iterator was read only this ugly const cast could go away
	assert(type == InstructionType::BranchTable);
	out << type.name() << " default: " << operandC << " [";
	auto it = branchTableVector(data);
	auto numLabels = it.nextU32();
	for (u32 i = 0; i != numLabels; i++) {
		out << " " << it.nextU32();
	}

	out << " ]";
}

void Instruction::printSelectVectorInstruction(std::ostream& out, const BufferSlice& data) const
{
	// FIXME: If buffer iterator was read only this ugly const cast could go away
	assert(type == InstructionType::SelectFrom);
	out << type.name() << " [";

	auto types = selectTypeVector(data);
	for (auto typeNum : types) {
		out << " " << ValType::fromInt(typeNum).name();
	}

	out << " ]";
}

bool InstructionType::isConstant() const
{
	using IT = InstructionType;
	switch (value) {
	case IT::I32Const:
	case IT::I64Const:
	case IT::F32Const:
	case IT::F64Const:
	case IT::ReferenceNull:
	case IT::ReferenceFunction:
	case IT::GlobalGet:
		return true;
	default:
		return false;
	}
}

bool InstructionType::isBinary() const
{
	using IT = InstructionType;
	switch (value) {
	case IT::I32Add:
	case IT::I32Subtract:
	case IT::I32Multiply:
	case IT::I32DivideS:
	case IT::I32DivideU:
	case IT::I32RemainderS:
	case IT::I32RemainderU:
	case IT::I32And:
	case IT::I32Or:
	case IT::I32Xor:
	case IT::I32ShiftLeft:
	case IT::I32ShiftRightS:
	case IT::I32ShiftRightU:
	case IT::I32RotateLeft:
	case IT::I32RotateRight:
	case IT::I64Add:
	case IT::I64Subtract:
	case IT::I64Multiply:
	case IT::I64DivideS:
	case IT::I64DivideU:
	case IT::I64RemainderS:
	case IT::I64RemainderU:
	case IT::I64And:
	case IT::I64Or:
	case IT::I64Xor:
	case IT::I64ShiftLeft:
	case IT::I64ShiftRightS:
	case IT::I64ShiftRightU:
	case IT::I64RotateLeft:
	case IT::I64RotateRight:
	case IT::F32Add:
	case IT::F32Subtract:
	case IT::F32Multiply:
	case IT::F32Divide:
	case IT::F32Minimum:
	case IT::F32Maximum:
	case IT::F32CopySign:
	case IT::F64Add:
	case IT::F64Subtract:
	case IT::F64Multiply:
	case IT::F64Divide:
	case IT::F64Minimum:
	case IT::F64Maximum:
	case IT::F64CopySign:
	case IT::I32Equal:
	case IT::I32NotEqual:
	case IT::I32LesserS:
	case IT::I32LesserU:
	case IT::I32GreaterS:
	case IT::I32GreaterU:
	case IT::I32LesserEqualS:
	case IT::I32LesserEqualU:
	case IT::I32GreaterEqualS:
	case IT::I32GreaterEqualU:
	case IT::I64Equal:
	case IT::I64NotEqual:
	case IT::I64LesserS:
	case IT::I64LesserU:
	case IT::I64GreaterS:
	case IT::I64GreaterU:
	case IT::I64LesserEqualS:
	case IT::I64LesserEqualU:
	case IT::I64GreaterEqualS:
	case IT::I64GreaterEqualU:
	case IT::F32Equal:
	case IT::F32NotEqual:
	case IT::F32Lesser:
	case IT::F32Greater:
	case IT::F32LesserEqual:
	case IT::F32GreaterEqual:
	case IT::F64Equal:
	case IT::F64NotEqual:
	case IT::F64Lesser:
	case IT::F64Greater:
	case IT::F64LesserEqual:
	case IT::F64GreaterEqual:
		return true;
	default:
		return false;
	}
}

bool InstructionType::isUnary() const
{
	using IT = InstructionType;
	switch (value) {
	case IT::I32CountLeadingZeros:
	case IT::I32CountTrailingZeros:
	case IT::I32CountOnes:
	case IT::I64CountLeadingZeros:
	case IT::I64CountTrailingZeros:
	case IT::I64CountOnes:
	case IT::F32Absolute:
	case IT::F32Negate:
	case IT::F32SquareRoot:
	case IT::F32Ceil:
	case IT::F32Floor:
	case IT::F32Truncate:
	case IT::F32Nearest:
	case IT::F64Absolute:
	case IT::F64Negate:
	case IT::F64SquareRoot:
	case IT::F64Ceil:
	case IT::F64Floor:
	case IT::F64Truncate:
	case IT::F64Nearest:
	case IT::I32EqualZero:
	case IT::I64EqualZero:
	case IT::I32WrapI64:
	case IT::I32TruncateF32S:
	case IT::I32TruncateF32U:
	case IT::I32TruncateF64S:
	case IT::I32TruncateF64U:
	case IT::I64ExtendI32S:
	case IT::I64ExtendI32U:
	case IT::I64TruncateF32S:
	case IT::I64TruncateF32U:
	case IT::I64TruncateF64S:
	case IT::I64TruncateF64U:
	case IT::F32ConvertI32S:
	case IT::F32ConvertI32U:
	case IT::F32ConvertI64S:
	case IT::F32ConvertI64U:
	case IT::F32DemoteF64:
	case IT::F64ConvertI32S:
	case IT::F64ConvertI32U:
	case IT::F64ConvertI64S:
	case IT::F64ConvertI64U:
	case IT::F64PromoteF32:
	case IT::I32ReinterpretF32:
	case IT::I64ReinterpretF64:
	case IT::F32ReinterpretI32:
	case IT::F64ReinterpretI64:
	case IT::I32Extend8s:
	case IT::I32Extend16s:
	case IT::I64Extend8s:
	case IT::I64Extend16s:
	case IT::I64Extend32s:
	case IT::I32TruncateSaturateF32S:
	case IT::I32TruncateSaturateF32U:
	case IT::I32TruncateSaturateF64S:
	case IT::I32TruncateSaturateF64U:
	case IT::I64TruncateSaturateF32S:
	case IT::I64TruncateSaturateF32U:
	case IT::I64TruncateSaturateF64S:
	case IT::I64TruncateSaturateF64U:
		return true;
	default:
		return false;
	}
}

bool InstructionType::isBlock() const
{
	switch (value) {
	case InstructionType::Block:
	case InstructionType::Loop:
	case InstructionType::If:
		return true;
	default:
		return false;
	}
}

bool InstructionType::isMemory() const
{
	switch(value) {
	case I32Load:
	case I64Load:
	case F32Load:
	case F64Load:
	case I32Load8s:
	case I32Load8u:
	case I32Load16s:
	case I32Load16u:
	case I64Load8s:
	case I64Load8u:
	case I64Load16s:
	case I64Load16u:
	case I64Load32s:
	case I64Load32u:
	case I32Store:
	case I64Store:
	case F32Store:
	case F64Store:
	case I32Store8:
	case I32Store16:
	case I64Store8:
	case I64Store16:
	case I64Store32:
		return true;
	default:
		return false;
	}
}

bool InstructionType::requiresMemoryInstance() const
{
	switch (value) {
	case I32Load:
	case I64Load:
	case F32Load:
	case F64Load:
	case I32Load8s:
	case I32Load8u:
	case I32Load16s:
	case I32Load16u:
	case I64Load8s:
	case I64Load8u:
	case I64Load16s:
	case I64Load16u:
	case I64Load32s:
	case I64Load32u:
	case I32Store:
	case I64Store:
	case F32Store:
	case F64Store:
	case I32Store8:
	case I32Store16:
	case I64Store8:
	case I64Store16:
	case I64Store32:
	case MemorySize:
	case MemoryGrow:
	case MemoryInit:
	case MemoryCopy:
	case MemoryFill:
		return true;
	default:
		return false;
	}
}

bool InstructionType::isBitCastConversionOnly() const
{
	switch (value) {
	case I32ReinterpretF32:
	case I64ReinterpretF64:
	case F32ReinterpretI32:
	case F64ReinterpretI64:
		return true;
	default:
		return false;
	}
}

std::optional<ValType> InstructionType::resultType() const
{
	using IT = InstructionType;
	switch (value) {
	case IT::I32Add:
	case IT::I32Subtract:
	case IT::I32Multiply:
	case IT::I32DivideS:
	case IT::I32DivideU:
	case IT::I32RemainderS:
	case IT::I32RemainderU:
	case IT::I32And:
	case IT::I32Or:
	case IT::I32Xor:
	case IT::I32ShiftLeft:
	case IT::I32ShiftRightS:
	case IT::I32ShiftRightU:
	case IT::I32RotateLeft:
	case IT::I32RotateRight:
		return ValType::I32;
	case IT::I64Add:
	case IT::I64Subtract:
	case IT::I64Multiply:
	case IT::I64DivideS:
	case IT::I64DivideU:
	case IT::I64RemainderS:
	case IT::I64RemainderU:
	case IT::I64And:
	case IT::I64Or:
	case IT::I64Xor:
	case IT::I64ShiftLeft:
	case IT::I64ShiftRightS:
	case IT::I64ShiftRightU:
	case IT::I64RotateLeft:
	case IT::I64RotateRight:
		return ValType::I64;
	case IT::F32Add:
	case IT::F32Subtract:
	case IT::F32Multiply:
	case IT::F32Divide:
	case IT::F32Minimum:
	case IT::F32Maximum:
	case IT::F32CopySign:
		return ValType::F32;
	case IT::F64Add:
	case IT::F64Subtract:
	case IT::F64Multiply:
	case IT::F64Divide:
	case IT::F64Minimum:
	case IT::F64Maximum:
	case IT::F64CopySign:
		return ValType::F64;
	case IT::I32Equal:
	case IT::I32NotEqual:
	case IT::I32LesserS:
	case IT::I32LesserU:
	case IT::I32GreaterS:
	case IT::I32GreaterU:
	case IT::I32LesserEqualS:
	case IT::I32LesserEqualU:
	case IT::I32GreaterEqualS:
	case IT::I32GreaterEqualU:
	case IT::I64Equal:
	case IT::I64NotEqual:
	case IT::I64LesserS:
	case IT::I64LesserU:
	case IT::I64GreaterS:
	case IT::I64GreaterU:
	case IT::I64LesserEqualS:
	case IT::I64LesserEqualU:
	case IT::I64GreaterEqualS:
	case IT::I64GreaterEqualU:
	case IT::F32Equal:
	case IT::F32NotEqual:
	case IT::F32Lesser:
	case IT::F32Greater:
	case IT::F32LesserEqual:
	case IT::F32GreaterEqual:
	case IT::F64Equal:
	case IT::F64NotEqual:
	case IT::F64Lesser:
	case IT::F64Greater:
	case IT::F64LesserEqual:
	case IT::F64GreaterEqual:
		return ValType::I32;
	case IT::I32CountLeadingZeros:
	case IT::I32CountTrailingZeros:
	case IT::I32CountOnes:
		return ValType::I32;
	case IT::I64CountLeadingZeros:
	case IT::I64CountTrailingZeros:
	case IT::I64CountOnes:
		return ValType::I64;
	case IT::F32Absolute:
	case IT::F32Negate:
	case IT::F32SquareRoot:
	case IT::F32Ceil:
	case IT::F32Floor:
	case IT::F32Truncate:
	case IT::F32Nearest:
		return ValType::F32;
	case IT::F64Absolute:
	case IT::F64Negate:
	case IT::F64SquareRoot:
	case IT::F64Ceil:
	case IT::F64Floor:
	case IT::F64Truncate:
	case IT::F64Nearest:
		return ValType::F64;
	case IT::I32EqualZero:
	case IT::I64EqualZero:
		return ValType::I32;
	case IT::I32Const:
		return ValType::I32;
	case IT::I64Const:
		return ValType::I64;
	case IT::F32Const:
		return ValType::F32;
	case IT::F64Const:
		return ValType::F64;
	case IT::ReferenceNull:
	case IT::ReferenceFunction:
		return ValType::FuncRef;
	case I32Load:
	case I32Load8s:
	case I32Load8u:
	case I32Load16s:
	case I32Load16u:
		return ValType::I32;
	case I64Load:
	case I64Load8s:
	case I64Load8u:
	case I64Load16s:
	case I64Load16u:
	case I64Load32s:
	case I64Load32u:
		return ValType::I64;
	case F32Load:
		return ValType::F32;
	case F64Load:
		return ValType::F64;
	case IT::I32WrapI64:
	case IT::I32TruncateF32S:
	case IT::I32TruncateF32U:
	case IT::I32TruncateF64S:
	case IT::I32TruncateF64U:
		return ValType::I32;
	case IT::I64ExtendI32S:
	case IT::I64ExtendI32U:
	case IT::I64TruncateF32S:
	case IT::I64TruncateF32U:
	case IT::I64TruncateF64S:
	case IT::I64TruncateF64U:
		return ValType::I64;
	case IT::F32ConvertI32S:
	case IT::F32ConvertI32U:
	case IT::F32ConvertI64S:
	case IT::F32ConvertI64U:
	case IT::F32DemoteF64:
		return ValType::F32;
	case IT::F64ConvertI32S:
	case IT::F64ConvertI32U:
	case IT::F64ConvertI64S:
	case IT::F64ConvertI64U:
	case IT::F64PromoteF32:
		return ValType::F64;
	case IT::I32ReinterpretF32:
		return ValType::I32;
	case IT::I64ReinterpretF64:
		return ValType::I64;
	case IT::F32ReinterpretI32:
		return ValType::F32;
	case IT::F64ReinterpretI64:
		return ValType::F64;
	case IT::I32Extend8s:
	case IT::I32Extend16s:
		return ValType::I32;
	case IT::I64Extend8s:
	case IT::I64Extend16s:
	case IT::I64Extend32s:
		return ValType::I64;
	case IT::I32TruncateSaturateF32S:
	case IT::I32TruncateSaturateF32U:
	case IT::I32TruncateSaturateF64S:
	case IT::I32TruncateSaturateF64U:
		return ValType::I32;
	case IT::I64TruncateSaturateF32S:
	case IT::I64TruncateSaturateF32U:
	case IT::I64TruncateSaturateF64S:
	case IT::I64TruncateSaturateF64U:
		return ValType::I64;
	default:
		return {};
	}
}


std::optional<ValType> InstructionType::operandType() const
{
	using IT = InstructionType;
	switch (value) {
	case IT::I32Add:
	case IT::I32Subtract:
	case IT::I32Multiply:
	case IT::I32DivideS:
	case IT::I32DivideU:
	case IT::I32RemainderS:
	case IT::I32RemainderU:
	case IT::I32And:
	case IT::I32Or:
	case IT::I32Xor:
	case IT::I32ShiftLeft:
	case IT::I32ShiftRightS:
	case IT::I32ShiftRightU:
	case IT::I32RotateLeft:
	case IT::I32RotateRight:
		return ValType::I32;
	case IT::I64Add:
	case IT::I64Subtract:
	case IT::I64Multiply:
	case IT::I64DivideS:
	case IT::I64DivideU:
	case IT::I64RemainderS:
	case IT::I64RemainderU:
	case IT::I64And:
	case IT::I64Or:
	case IT::I64Xor:
	case IT::I64ShiftLeft:
	case IT::I64ShiftRightS:
	case IT::I64ShiftRightU:
	case IT::I64RotateLeft:
	case IT::I64RotateRight:
		return ValType::I64;
	case IT::F32Add:
	case IT::F32Subtract:
	case IT::F32Multiply:
	case IT::F32Divide:
	case IT::F32Minimum:
	case IT::F32Maximum:
	case IT::F32CopySign:
		return ValType::F32;
	case IT::F64Add:
	case IT::F64Subtract:
	case IT::F64Multiply:
	case IT::F64Divide:
	case IT::F64Minimum:
	case IT::F64Maximum:
	case IT::F64CopySign:
		return ValType::F64;
	case IT::I32Equal:
	case IT::I32NotEqual:
	case IT::I32LesserS:
	case IT::I32LesserU:
	case IT::I32GreaterS:
	case IT::I32GreaterU:
	case IT::I32LesserEqualS:
	case IT::I32LesserEqualU:
	case IT::I32GreaterEqualS:
	case IT::I32GreaterEqualU:
		return ValType::I32;
	case IT::I64Equal:
	case IT::I64NotEqual:
	case IT::I64LesserS:
	case IT::I64LesserU:
	case IT::I64GreaterS:
	case IT::I64GreaterU:
	case IT::I64LesserEqualS:
	case IT::I64LesserEqualU:
	case IT::I64GreaterEqualS:
	case IT::I64GreaterEqualU:
		return ValType::I64;
	case IT::F32Equal:
	case IT::F32NotEqual:
	case IT::F32Lesser:
	case IT::F32Greater:
	case IT::F32LesserEqual:
	case IT::F32GreaterEqual:
		return ValType::F32;
	case IT::F64Equal:
	case IT::F64NotEqual:
	case IT::F64Lesser:
	case IT::F64Greater:
	case IT::F64LesserEqual:
	case IT::F64GreaterEqual:
		return ValType::F64;
	case IT::I32CountLeadingZeros:
	case IT::I32CountTrailingZeros:
	case IT::I32CountOnes:
		return ValType::I32;
	case IT::I64CountLeadingZeros:
	case IT::I64CountTrailingZeros:
	case IT::I64CountOnes:
		return ValType::I64;
	case IT::F32Absolute:
	case IT::F32Negate:
	case IT::F32SquareRoot:
	case IT::F32Ceil:
	case IT::F32Floor:
	case IT::F32Truncate:
	case IT::F32Nearest:
		return ValType::F32;
	case IT::F64Absolute:
	case IT::F64Negate:
	case IT::F64SquareRoot:
	case IT::F64Ceil:
	case IT::F64Floor:
	case IT::F64Truncate:
	case IT::F64Nearest:
		return ValType::F64;
	case IT::I32EqualZero:
		return ValType::I32;
	case IT::I32Store:
	case IT::I32Store8:
	case IT::I32Store16:
		return ValType::I32;
	case IT::I64Store:
	case IT::I64Store8:
	case IT::I64Store16:
	case IT::I64Store32:
		return ValType::I64;
	case IT::F64Store:
		return ValType::F32;
	case IT::F32Store:
		return ValType::F64;
	case IT::I32WrapI64:
		return ValType::I64;
	case IT::I32TruncateF32S:
		return ValType::F32;
	case IT::I32TruncateF32U:
		return ValType::F32;
	case IT::I32TruncateF64S:
		return ValType::F64;
	case IT::I32TruncateF64U:
		return ValType::F64;
	case IT::I64ExtendI32S:
	case IT::I64ExtendI32U:
		return ValType::I32;
	case IT::I64TruncateF32S:
	case IT::I64TruncateF32U:
		return ValType::F32;
	case IT::I64TruncateF64S:
	case IT::I64TruncateF64U:
		return ValType::F64;
	case IT::F32ConvertI32S:
	case IT::F32ConvertI32U:
		return ValType::I32;
	case IT::F32ConvertI64S:
	case IT::F32ConvertI64U:
		return ValType::I64;
	case IT::F32DemoteF64:
		return ValType::F64;
	case IT::F64ConvertI32S:
	case IT::F64ConvertI32U:
		return ValType::I32;
	case IT::F64ConvertI64S:
	case IT::F64ConvertI64U:
		return ValType::I64;
	case IT::F64PromoteF32:
	case IT::I32ReinterpretF32:
		return ValType::F32;
	case IT::I64ReinterpretF64:
		return ValType::F64;
	case IT::F32ReinterpretI32:
		return ValType::I32;
	case IT::F64ReinterpretI64:
		return ValType::I64;
	case IT::I32Extend8s:
	case IT::I32Extend16s:
		return ValType::I32;
	case IT::I64Extend8s:
	case IT::I64Extend16s:
	case IT::I64Extend32s:
		return ValType::I64;
	case IT::I32TruncateSaturateF32S:
	case IT::I32TruncateSaturateF32U:
		return ValType::F32;
	case IT::I32TruncateSaturateF64S:
	case IT::I32TruncateSaturateF64U:
		return ValType::F64;
	case IT::I64TruncateSaturateF32S:
	case IT::I64TruncateSaturateF32U:
		return ValType::F32;
	case IT::I64TruncateSaturateF64S:
	case IT::I64TruncateSaturateF64U:
		return ValType::F64;
	default:
		return {};
	} 
}


void Instruction::printBlockTypeInstruction(std::ostream& out) const
{
	assert(type == InstructionType::Block || type == InstructionType::Loop || type == InstructionType::If);
	auto blockType = BlockType::fromInt(operandA);
	out << type.name() << " " << blockType.name();

	if (blockType == BlockType::ValType) {
		auto valType = ValType::fromInt(operandB);
		assert(valType.isValid());
		out << " " << valType.name();
		return;
	}

	if (blockType == BlockType::TypeIndex) {
		out << " " << operandB;
	}
}


void Instruction::print(std::ostream& out, const BufferSlice& data) const
{
	using IT = InstructionType;
	switch (type) {
	case IT::Unreachable:
	case IT::NoOperation:
		out << type.name();
		break;
	case IT::Block:
	case IT::Loop:
	case IT::If:
		printBlockTypeInstruction(out);
		break;
	case IT::Else:
	case IT::End:
		out << type.name();
		break;
	case IT::Branch:
	case IT::BranchIf:
		out << type.name() << " Label: " << operandA;
		break;
	case IT::BranchTable:
		printBranchTableInstruction(out, data);
		break;
	case IT::Return:
		out << type.name();
		break;
	case IT::Call: 
		out << type.name() << " Function: " << operandA;
		break;
	case IT::CallIndirect: 
		out << type.name() << " Type: " << operandA << " Table: " << operandB;
		break;
	case IT::Drop:
	case IT::Select:
		out << type.name();
		break;
	case IT::SelectFrom:
		printSelectVectorInstruction(out, data);
	case IT::LocalGet:
	case IT::LocalSet:
	case IT::LocalTee:
	case IT::GlobalGet:
	case IT::GlobalSet: 
		out << type.name() << " " << operandA;
		break;
	case IT::ReferenceNull:
		out << type.name() << " Type: " << ValType::fromInt(operandA).name();
		break;
	case IT::ReferenceIsNull:
		out << type.name();
		break;
	case IT::ReferenceFunction:
		out << type.name() << " Function: " << operandA;
		break;
	case IT::TableGet:
	case IT::TableSet:
	case IT::TableGrow:
	case IT::TableSize:
	case IT::TableFill:
		out << type.name() << " Table: " << operandA;
		break;
	case IT::ElementDrop:
		out << type.name() << " Element: " << operandA;
		break;
	case IT::TableInit:
		out << type.name() << " Element: " << operandA << " Table: " << operandB;
		break;
	case IT::TableCopy:
		out << type.name() << " Table: " << operandA << " <- Table: " << operandB;
		break;
	case IT::I32Load:
	case IT::I64Load:
	case IT::F32Load:
	case IT::F64Load:
	case IT::I32Load8s:
	case IT::I32Load8u:
	case IT::I32Load16s:
	case IT::I32Load16u:
	case IT::I64Load8s:
	case IT::I64Load8u:
	case IT::I64Load16s:
	case IT::I64Load16u:
	case IT::I64Load32s:
	case IT::I64Load32u:
	case IT::I32Store:
	case IT::I64Store:
	case IT::F32Store:
	case IT::F64Store:
	case IT::I32Store8:
	case IT::I32Store16:
	case IT::I64Store8:
	case IT::I64Store16:
	case IT::I64Store32:
		out << type.name() << " Alignment: " << operandA << " Offset: " << operandB;
		break;
	case IT::MemorySize:
	case IT::MemoryGrow:
		out << type.name() << " (implicitly memory 0)";
		break;
	case IT::MemoryInit:
		out << type.name() << " Data: " << operandA <<  " (implicitly memory 0)";
		break;
	case IT::DataDrop:
		out << type.name() << " Data: " << operandA;
		break;
	case IT::MemoryCopy:
	case IT::MemoryFill:
		out << type.name() << " (implicitly memory 0)";
		break;
	case IT::I32Const:
		out << type.name() << " " << i32Constant;
		break;
	case IT::I64Const:
		out << type.name() << " " << i64Constant;
		break;
	case IT::F32Const:
		out << type.name() << " " << f32Constant;
		break;
	case IT::F64Const:
		out << type.name() << " " << f64Constant;
		break;
	case IT::I32EqualZero:
	case IT::I32Equal:
	case IT::I32NotEqual:
	case IT::I32LesserS:
	case IT::I32LesserU:
	case IT::I32GreaterS:
	case IT::I32GreaterU:
	case IT::I32LesserEqualS:
	case IT::I32LesserEqualU:
	case IT::I32GreaterEqualS:
	case IT::I32GreaterEqualU:
	case IT::I64EqualZero:
	case IT::I64Equal:
	case IT::I64NotEqual:
	case IT::I64LesserS:
	case IT::I64LesserU:
	case IT::I64GreaterS:
	case IT::I64GreaterU:
	case IT::I64LesserEqualS:
	case IT::I64LesserEqualU:
	case IT::I64GreaterEqualS:
	case IT::I64GreaterEqualU:
	case IT::F32Equal:
	case IT::F32NotEqual:
	case IT::F32Lesser:
	case IT::F32Greater:
	case IT::F32LesserEqual:
	case IT::F32GreaterEqual:
	case IT::F64Equal:
	case IT::F64NotEqual:
	case IT::F64Lesser:
	case IT::F64Greater:
	case IT::F64LesserEqual:
	case IT::F64GreaterEqual:
	case IT::I32CountLeadingZeros:
	case IT::I32CountTrailingZeros:
	case IT::I32CountOnes:
	case IT::I32Add:
	case IT::I32Subtract:
	case IT::I32Multiply:
	case IT::I32DivideS:
	case IT::I32DivideU:
	case IT::I32RemainderS:
	case IT::I32RemainderU:
	case IT::I32And:
	case IT::I32Or:
	case IT::I32Xor:
	case IT::I32ShiftLeft:
	case IT::I32ShiftRightS:
	case IT::I32ShiftRightU:
	case IT::I32RotateLeft:
	case IT::I32RotateRight:
	case IT::I64CountLeadingZeros:
	case IT::I64CountTrailingZeros:
	case IT::I64CountOnes:
	case IT::I64Add:
	case IT::I64Subtract:
	case IT::I64Multiply:
	case IT::I64DivideS:
	case IT::I64DivideU:
	case IT::I64RemainderS:
	case IT::I64RemainderU:
	case IT::I64And:
	case IT::I64Or:
	case IT::I64Xor:
	case IT::I64ShiftLeft:
	case IT::I64ShiftRightS:
	case IT::I64ShiftRightU:
	case IT::I64RotateLeft:
	case IT::I64RotateRight:
	case IT::F32Absolute:
	case IT::F32Negate:
	case IT::F32Ceil:
	case IT::F32Floor:
	case IT::F32Truncate:
	case IT::F32Nearest:
	case IT::F32SquareRoot:
	case IT::F32Add:
	case IT::F32Subtract:
	case IT::F32Multiply:
	case IT::F32Divide:
	case IT::F32Minimum:
	case IT::F32Maximum:
	case IT::F32CopySign:
	case IT::F64Absolute:
	case IT::F64Negate:
	case IT::F64Ceil:
	case IT::F64Floor:
	case IT::F64Truncate:
	case IT::F64Nearest:
	case IT::F64SquareRoot:
	case IT::F64Add:
	case IT::F64Subtract:
	case IT::F64Multiply:
	case IT::F64Divide:
	case IT::F64Minimum:
	case IT::F64Maximum:
	case IT::F64CopySign:
	case IT::I32WrapI64:
	case IT::I32TruncateF32S:
	case IT::I32TruncateF32U:
	case IT::I32TruncateF64S:
	case IT::I32TruncateF64U:
	case IT::I64ExtendI32S:
	case IT::I64ExtendI32U:
	case IT::I64TruncateF32S:
	case IT::I64TruncateF32U:
	case IT::I64TruncateF64S:
	case IT::I64TruncateF64U:
	case IT::F32ConvertI32S:
	case IT::F32ConvertI32U:
	case IT::F32ConvertI64S:
	case IT::F32ConvertI64U:
	case IT::F32DemoteF64:
	case IT::F64ConvertI32S:
	case IT::F64ConvertI32U:
	case IT::F64ConvertI64S:
	case IT::F64ConvertI64U:
	case IT::F64PromoteF32:
	case IT::I32ReinterpretF32:
	case IT::I64ReinterpretF64:
	case IT::F32ReinterpretI32:
	case IT::F64ReinterpretI64:
	case IT::I32Extend8s:
	case IT::I32Extend16s:
	case IT::I64Extend8s:
	case IT::I64Extend16s:
	case IT::I64Extend32s:
	case IT::I32TruncateSaturateF32S:
	case IT::I32TruncateSaturateF32U:
	case IT::I32TruncateSaturateF64S:
	case IT::I32TruncateSaturateF64U:
	case IT::I64TruncateSaturateF32S:
	case IT::I64TruncateSaturateF32U:
	case IT::I64TruncateSaturateF64S:
	case IT::I64TruncateSaturateF64U:
		out << type.name();
		break;
	default:
		throw std::runtime_error{ "Could not create instruction object from unkown instruction type" };
	}
}

BlockTypeIndex Instruction::blockTypeIndex() const
{
	assert(type.isBlock());
	return { BlockType::fromInt(operandA), ModuleTypeIndex{ operandB } };
}

u32 Instruction::branchLabel() const {
	// assert(isBranch());
	return operandA;
}

u32 Instruction::branchTableDefaultLabel() const
{
	assert(type == InstructionType::BranchTable);
	return operandC;
}

u32 Instruction::localIndex() const {
	// assert(isGetterSetter());
	return operandA;
}

ModuleGlobalIndex Instruction::globalIndex() const
{
	assert(type == InstructionType::GlobalGet || type == InstructionType::GlobalSet);
	return ModuleGlobalIndex{ operandA };
}

ModuleFunctionIndex Instruction::functionIndex() const {
	assert(type == InstructionType::Call || type == InstructionType::CallIndirect);
	return ModuleFunctionIndex{ operandA };
}

u32 Instruction::memoryOffset() const
{
	assert(type.isMemory());
	return operandB;
}

u32 Instruction::dataSegmentIndex() const
{
	assert(type == InstructionType::MemoryInit || type == InstructionType::DataDrop);
	return operandA;
}

ModuleTableIndex Instruction::callTableIndex() const
{
	assert(type == InstructionType::CallIndirect);
	return ModuleTableIndex{ operandB };
}

ModuleElementIndex Instruction::elementIndex() const
{
	assert(type == InstructionType::TableInit || type == InstructionType::ElementDrop);
	return ModuleElementIndex{ operandB };
}

ModuleTableIndex Instruction::tableIndex() const
{
	// assert(isTableInstruction());
	return ModuleTableIndex{ operandA };
}

ModuleTableIndex WASM::Instruction::sourceTableIndex() const
{
	assert(type == InstructionType::TableCopy);
	return ModuleTableIndex{ operandB };
}

i32 Instruction::asI32Constant() const
{
	assert(type == InstructionType::I32Const);
	return i32Constant;
}

u32 Instruction::asIF32Constant() const
{
	assert(type == InstructionType::I32Const || type == InstructionType::F32Const); 
	return i32Constant;
}

u64 Instruction::asIF64Constant() const
{
	assert(type == InstructionType::I64Const || type == InstructionType::F64Const);
	return i64Constant;
}

std::optional<ModuleFunctionIndex> Instruction::asReferenceIndex() const
{
	assert(type == InstructionType::ReferenceFunction || type == InstructionType::ReferenceNull);
	if (type == InstructionType::ReferenceFunction) {
		return ModuleFunctionIndex{ operandA };
	}

	return {};
}

std::span<const u8> Instruction::selectTypeVector(const BufferSlice& data) const
{
	assert(type == InstructionType::SelectFrom);
	auto it = const_cast<BufferSlice&>(data).iterator();
	it.moveTo(vectorPointer);
	auto numTypes = it.nextU32();
	return { it.positionPointer(), numTypes };
}

BufferIterator Instruction::branchTableVector(const BufferSlice& data) const
{
	assert(type == InstructionType::BranchTable);
	auto it = const_cast<BufferSlice&>(data).iterator();
	it.moveTo(vectorPointer);
	return it;
}

std::optional<Bytecode> Instruction::toBytecode() const
{
	using IT = InstructionType;
	using BA = Bytecode;
	switch (type) {
		case IT::Unreachable: return BA::Unreachable;
		case IT::NoOperation:
		case IT::Block:
		case IT::Loop:
		case IT::If:
		case IT::Else:
		case IT::End:
		case IT::Branch:
		case IT::BranchIf:
		case IT::BranchTable:
			return {};
		case IT::Return: return {};
		case IT::Call: return BA::Call;
		case IT::CallIndirect: return BA::CallIndirect;
		case IT::Drop:
		case IT::Select:
		case IT::SelectFrom:
		case IT::LocalGet:
		case IT::LocalSet:
		case IT::LocalTee:
			return {};
		case IT::GlobalGet:
		case IT::GlobalSet:
		case IT::ReferenceNull:
		case IT::ReferenceIsNull:
		case IT::ReferenceFunction:
			return {};
		case IT::TableGet: return BA::TableGet;
		case IT::TableSet: return BA::TableSet;
		case IT::TableInit: return BA::TableInit;
		case IT::ElementDrop: return BA::ElementDrop;
		case IT::TableCopy: return BA::TableCopy;
		case IT::TableGrow: return BA::TableGrow;
		case IT::TableSize: return BA::TableSize;
		case IT::TableFill: return BA::TableFill;
		case IT::I32Load:
		case IT::I64Load:
		case IT::F32Load:
		case IT::F64Load:
			return {};
		case IT::I32Load8s: return BA::I32Load8s;
		case IT::I32Load8u: return BA::I32Load8u;
		case IT::I32Load16s: return BA::I32Load16s;
		case IT::I32Load16u: return BA::I32Load16u;
		case IT::I64Load8s: return BA::I64Load8s;
		case IT::I64Load8u: return BA::I64Load8u;
		case IT::I64Load16s: return BA::I64Load16s;
		case IT::I64Load16u: return BA::I64Load16u;
		case IT::I64Load32s: return BA::I64Load32s;
		case IT::I64Load32u: return BA::I64Load32u;
		case IT::I32Store:
		case IT::I64Store:
		case IT::F32Store:
		case IT::F64Store:
			return {};
		case IT::I32Store8: return BA::I32Store8;
		case IT::I32Store16: return BA::I32Store16;
		case IT::I64Store8: return BA::I64Store8;
		case IT::I64Store16: return BA::I64Store16;
		case IT::I64Store32: return BA::I64Store32;
		case IT::MemorySize: return BA::MemorySize;
		case IT::MemoryGrow: return BA::MemoryGrow;
		case IT::MemoryInit: return BA::MemoryInit;
		case IT::DataDrop: return BA::DataDrop;
		case IT::MemoryCopy: return BA::MemoryCopy;
		case IT::MemoryFill: return BA::MemoryFill;
		case IT::I32Const:
		case IT::I64Const:
			return {};
		case IT::F32Const: return BA::I32ConstLong;
		case IT::F64Const: return BA::I64ConstLong;
		case IT::I32EqualZero: return BA::I32EqualZero;
		case IT::I32Equal: return BA::I32Equal;
		case IT::I32NotEqual: return BA::I32NotEqual;
		case IT::I32LesserS: return BA::I32LesserS;
		case IT::I32LesserU: return BA::I32LesserU;
		case IT::I32GreaterS: return BA::I32GreaterS;
		case IT::I32GreaterU: return BA::I32GreaterU;
		case IT::I32LesserEqualS: return BA::I32LesserEqualS;
		case IT::I32LesserEqualU: return BA::I32LesserEqualU;
		case IT::I32GreaterEqualS: return BA::I32GreaterEqualS;
		case IT::I32GreaterEqualU: return BA::I32GreaterEqualU;
		case IT::I64EqualZero: return BA::I64EqualZero;
		case IT::I64Equal: return BA::I64Equal;
		case IT::I64NotEqual: return BA::I64NotEqual;
		case IT::I64LesserS: return BA::I64LesserS;
		case IT::I64LesserU: return BA::I64LesserU;
		case IT::I64GreaterS: return BA::I64GreaterS;
		case IT::I64GreaterU: return BA::I64GreaterU;
		case IT::I64LesserEqualS: return BA::I64LesserEqualS;
		case IT::I64LesserEqualU: return BA::I64LesserEqualU;
		case IT::I64GreaterEqualS: return BA::I64GreaterEqualS;
		case IT::I64GreaterEqualU: return BA::I64GreaterEqualU;
		case IT::F32Equal: return BA::F32Equal;
		case IT::F32NotEqual: return BA::F32NotEqual;
		case IT::F32Lesser: return BA::F32Lesser;
		case IT::F32Greater: return BA::F32Greater;
		case IT::F32LesserEqual: return BA::F32LesserEqual;
		case IT::F32GreaterEqual: return BA::F32GreaterEqual;
		case IT::F64Equal: return BA::F64Equal;
		case IT::F64NotEqual: return BA::F64NotEqual;
		case IT::F64Lesser: return BA::F64Lesser;
		case IT::F64Greater: return BA::F64Greater;
		case IT::F64LesserEqual: return BA::F64LesserEqual;
		case IT::F64GreaterEqual: return BA::F64GreaterEqual;
		case IT::I32CountLeadingZeros: return BA::I32CountLeadingZeros;
		case IT::I32CountTrailingZeros: return BA::I32CountTrailingZeros;
		case IT::I32CountOnes: return BA::I32CountOnes;
		case IT::I32Add: return BA::I32Add;
		case IT::I32Subtract: return BA::I32Subtract;
		case IT::I32Multiply: return BA::I32Multiply;
		case IT::I32DivideS: return BA::I32DivideS;
		case IT::I32DivideU: return BA::I32DivideU;
		case IT::I32RemainderS: return BA::I32RemainderS;
		case IT::I32RemainderU: return BA::I32RemainderU;
		case IT::I32And: return BA::I32And;
		case IT::I32Or: return BA::I32Or;
		case IT::I32Xor: return BA::I32Xor;
		case IT::I32ShiftLeft: return BA::I32ShiftLeft;
		case IT::I32ShiftRightS: return BA::I32ShiftRightS;
		case IT::I32ShiftRightU: return BA::I32ShiftRightU;
		case IT::I32RotateLeft: return BA::I32RotateLeft;
		case IT::I32RotateRight: return BA::I32RotateRight;
		case IT::I64CountLeadingZeros: return BA::I64CountLeadingZeros;
		case IT::I64CountTrailingZeros: return BA::I64CountTrailingZeros;
		case IT::I64CountOnes: return BA::I64CountOnes;
		case IT::I64Add: return BA::I64Add;
		case IT::I64Subtract: return BA::I64Subtract;
		case IT::I64Multiply: return BA::I64Multiply;
		case IT::I64DivideS: return BA::I64DivideS;
		case IT::I64DivideU: return BA::I64DivideU;
		case IT::I64RemainderS: return BA::I64RemainderS;
		case IT::I64RemainderU: return BA::I64RemainderU;
		case IT::I64And: return BA::I64And;
		case IT::I64Or: return BA::I64Or;
		case IT::I64Xor: return BA::I64Xor;
		case IT::I64ShiftLeft: return BA::I64ShiftLeft;
		case IT::I64ShiftRightS: return BA::I64ShiftRightS;
		case IT::I64ShiftRightU: return BA::I64ShiftRightU;
		case IT::I64RotateLeft: return BA::I64RotateLeft;
		case IT::I64RotateRight: return BA::I64RotateRight;
		case IT::F32Absolute: return BA::F32Absolute;
		case IT::F32Negate: return BA::F32Negate;
		case IT::F32Ceil: return BA::F32Ceil;
		case IT::F32Floor: return BA::F32Floor;
		case IT::F32Truncate: return BA::F32Truncate;
		case IT::F32Nearest: return BA::F32Nearest;
		case IT::F32SquareRoot: return BA::F32SquareRoot;
		case IT::F32Add: return BA::F32Add;
		case IT::F32Subtract: return BA::F32Subtract;
		case IT::F32Multiply: return BA::F32Multiply;
		case IT::F32Divide: return BA::F32Divide;
		case IT::F32Minimum: return BA::F32Minimum;
		case IT::F32Maximum: return BA::F32Maximum;
		case IT::F32CopySign: return BA::F32CopySign;
		case IT::F64Absolute: return BA::F64Absolute;
		case IT::F64Negate: return BA::F64Negate;
		case IT::F64Ceil: return BA::F64Ceil;
		case IT::F64Floor: return BA::F64Floor;
		case IT::F64Truncate: return BA::F64Truncate;
		case IT::F64Nearest: return BA::F64Nearest;
		case IT::F64SquareRoot: return BA::F64SquareRoot;
		case IT::F64Add: return BA::F64Add;
		case IT::F64Subtract: return BA::F64Subtract;
		case IT::F64Multiply: return BA::F64Multiply;
		case IT::F64Divide: return BA::F64Divide;
		case IT::F64Minimum: return BA::F64Minimum;
		case IT::F64Maximum: return BA::F64Maximum;
		case IT::F64CopySign: return BA::F64CopySign;
		case IT::I32WrapI64: return BA::I32WrapI64;
		case IT::I32TruncateF32S: return BA::I32TruncateF32S;
		case IT::I32TruncateF32U: return BA::I32TruncateF32U;
		case IT::I32TruncateF64S: return BA::I32TruncateF64S;
		case IT::I32TruncateF64U: return BA::I32TruncateF64U;
		case IT::I64ExtendI32S: return BA::I64ExtendI32S;
		case IT::I64ExtendI32U: return BA::I64ExtendI32U;
		case IT::I64TruncateF32S: return BA::I64TruncateF32S;
		case IT::I64TruncateF32U: return BA::I64TruncateF32U;
		case IT::I64TruncateF64S: return BA::I64TruncateF64S;
		case IT::I64TruncateF64U: return BA::I64TruncateF64U;
		case IT::F32ConvertI32S: return BA::F32ConvertI32S;
		case IT::F32ConvertI32U: return BA::F32ConvertI32U;
		case IT::F32ConvertI64S: return BA::F32ConvertI64S;
		case IT::F32ConvertI64U: return BA::F32ConvertI64U;
		case IT::F32DemoteF64: return BA::F32DemoteF64;
		case IT::F64ConvertI32S: return BA::F64ConvertI32S;
		case IT::F64ConvertI32U: return BA::F64ConvertI32U;
		case IT::F64ConvertI64S: return BA::F64ConvertI64S;
		case IT::F64ConvertI64U: return BA::F64ConvertI64U;
		case IT::F64PromoteF32: return BA::F64PromoteF32;
		case IT::I32ReinterpretF32:
		case IT::I64ReinterpretF64:
		case IT::F32ReinterpretI32:
		case IT::F64ReinterpretI64:
			return {};
		case IT::I32Extend8s: return BA::I32Extend8s;
		case IT::I32Extend16s: return BA::I32Extend16s;
		case IT::I64Extend8s: return BA::I64Extend8s;
		case IT::I64Extend16s: return BA::I64Extend16s;
		case IT::I64Extend32s: return BA::I64Extend32s;
		case IT::I32TruncateSaturateF32S: return BA::I32TruncateSaturateF32S;
		case IT::I32TruncateSaturateF32U: return BA::I32TruncateSaturateF32U;
		case IT::I32TruncateSaturateF64S: return BA::I32TruncateSaturateF64S;
		case IT::I32TruncateSaturateF64U: return BA::I32TruncateSaturateF64U;
		case IT::I64TruncateSaturateF32S: return BA::I64TruncateSaturateF32S;
		case IT::I64TruncateSaturateF32U: return BA::I64TruncateSaturateF32U;
		case IT::I64TruncateSaturateF64S: return BA::I64TruncateSaturateF64S;
		case IT::I64TruncateSaturateF64U: return BA::I64TruncateSaturateF64U;
	}
}

u32 Instruction::maxPrintedByteLength(const BufferSlice& data) const
{
	auto bytecode = toBytecode();
	if (bytecode.has_value()) {
		return 1+ bytecode->arguments().sizeInBytes();
	}

	using IT = InstructionType;
	switch (type) {
	case IT::NoOperation:
	case IT::Block:
	case IT::Loop:
		return 0;
	case IT::If:
	case IT::Else:
		return 5; // Far jump
	case IT::End:
		return 0;
	case IT::Branch:
	case IT::BranchIf:
		return 5; // Far jump
	case IT::BranchTable: {
		auto numLabels = branchTableVector(data).nextU32();
		return 9 + numLabels * 4;
	}
	case IT::Return: return 5;
	case IT::Drop:
	case IT::Select:
	case IT::SelectFrom:
		return 1;
	case IT::LocalGet:
	case IT::LocalSet:
	case IT::LocalTee:
		return 5;
	case IT::GlobalGet:
	case IT::GlobalSet:
		return 9;
	case IT::ReferenceNull:
	case IT::ReferenceFunction:
		return 9;
	case IT::ReferenceIsNull:
		return 1;
	case IT::I32Load:
	case IT::I64Load:
	case IT::F32Load:
	case IT::F64Load:
	case IT::I32Store:
	case IT::I64Store:
	case IT::F32Store:
	case IT::F64Store:
		if (memoryOffset() <= 255) {
			return 2;
		}
		else {
			return 5;
		}
	case IT::I32ReinterpretF32:
	case IT::I64ReinterpretF64:
	case IT::F32ReinterpretI32:
	case IT::F64ReinterpretI64:
		return 0;
	case IT::I32Const:
		return 5;
	case IT::I64Const:
		return 9;
	default: assert(false);
	}
}

std::optional<ValType> InstructionType::constantType() const
{
	switch (value) {
	case InstructionType::I32Const: return ValType::I32;
	case InstructionType::I64Const: return ValType::I64;
	case InstructionType::F32Const: return ValType::F32;
	case InstructionType::F64Const: return ValType::F64;
	case InstructionType::ReferenceNull: return ValType::FuncRef;
	case InstructionType::ReferenceFunction: return ValType::FuncRef;
		// GlobalGet has to be handled manually be the caller
	}

	return {};
}

BlockTypeParameters BlockTypeIndex::parameters() const
{
	if (blockType == BlockType::TypeIndex) {
		return { index };
	}

	return {};
}

BlockTypeResults BlockTypeIndex::results() const
{
	return { blockType, index };
}
