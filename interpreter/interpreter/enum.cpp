
#include <cassert>

#include "enum.h"
#include "bytecode.h"

using namespace WASM;

const char* SectionType::name() const
{
	switch (value) {
	case Custom: return "Custom";
	case Type: return "Type";
	case Import: return "Import";
	case Function: return "Function";
	case Table: return "Table";
	case Memory: return "Memory";
	case GlobalType: return "GlobalType";
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

u32 ValType::sizeInBytes() const
{
	switch (value) {
	case I32: return 4;
	case I64: return 8;
	case F32: return 4;
	case F64: return 8;
	case V128: return 16;
	case FuncRef: return 8;
	case ExternRef: return 8;
	default:
		assert(false);
	}
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

const char* ElementMode::name() const
{
	switch (value) {
	case Passive: return "Passive";
	case Active: return "Active";
	case Declarative: return "Declarative";
	default: return "<unknown element mode>";
	}
}

const char* NameSubsectionType::name() const
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

const char* BlockType::name() const
{
	switch (value) {
	case None: return "None";
	case ValType: return "ValType";
	case TypeIndex: return "TypeIndex";
	default: return "<unknown block type>";
	}
}


const char* Bytecode::name() const {
	switch (value) {
		case Unreachable: return "Unreachable";
		case JumpShort: return "JumpShort";
		case JumpLong: return "JumpLong";
		case IfTrueJumpShort: return "IfTrueJumpShort";
		case IfTrueJumpLong: return "IfTrueJumpLong";
		case IfFalseJumpShort: return "IfFalseJumpShort";
		case IfFalseJumpLong: return "IfFalseJumpLong";
		case JumpTable: return "JumpTable";
		case ReturnFew: return "ReturnFew";
		case ReturnMany: return "ReturnMany";
		case Call: return "Call";
		case CallIndirect: return "CallIndirect";
		case Entry: return "Entry";
		case I32Drop: return "I32Drop";
		case I64Drop: return "I64Drop";
		case I32Select: return "I32Select";
		case I64Select: return "I64Select";
		case I32LocalGetFar: return "I32LocalGetFar";
		case I32LocalSetFar: return "I32LocalSetFar";
		case I32LocalTeeFar: return "I32LocalTeeFar";
		case I32LocalGetNear: return "I32LocalGetNear";
		case I32LocalSetNear: return "I32LocalSetNear";
		case I32LocalTeeNear: return "I32LocalTeeNear";
		case I64LocalGetFar: return "I64LocalGetFar";
		case I64LocalSetFar: return "I64LocalSetFar";
		case I64LocalTeeFar: return "I64LocalTeeFar";
		case I64LocalGetNear: return "I64LocalGetNear";
		case I64LocalSetNear: return "I64LocalSetNear";
		case I64LocalTeeNear: return "I64LocalTeeNear";
		case I32GlobalGet: return "I32GlobalGet";
		case I32GlobalSet: return "I32GlobalSet";
		case I64GlobalGet: return "I64GlobalGet";
		case I64GlobalSet: return "I64GlobalSet";
		case TableGet: return "TableGet";
		case TableSet: return "TableSet";
		case TableInit: return "TableInit";
		case ElementDrop: return "ElementDrop";
		case TableCopy: return "TableCopy";
		case TableGrow: return "TableGrow";
		case TableSize: return "TableSize";
		case TableFill: return "TableFill";
		case I32LoadNear: return "I32LoadNear";
		case I64LoadNear: return "I64LoadNear";
		case I32LoadFar: return "F32LoadFar";
		case I64LoadFar: return "F64LoadFar";
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
		case I32StoreNear: return "I32StoreNear";
		case I64StoreNear: return "I64StoreNear";
		case I32StoreFar: return "F32StoreFar";
		case I64StoreFar: return "F64StoreFar";
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
		default: return "<unknown byte code>";
	}
}

BytecodeArguments WASM::Bytecode::arguments() const
{
	using BA = BytecodeArguments;
	switch (value) {
	case Unreachable: return BA::None;
	case JumpShort: return BA::SingleU8;
	case JumpLong: return BA::SingleU32;
	case IfTrueJumpShort:
	case IfFalseJumpShort:
		return BA::SingleU8;
	case IfTrueJumpLong:
	case IfFalseJumpLong:
		return BA::SingleU32;
	case JumpTable: return BA::DualU32;
	case ReturnFew: return BA::SingleU8;
	case ReturnMany: return BA::SingleU32;
	case Call: return BA::SingleU64SingleU32;
	case CallIndirect: return BA::DualU32;
	case Entry: return BA::SingleU64SingleU32;
	case I32Drop:
	case I64Drop:
	case I32Select:
	case I64Select:
		return BA::None;
	case I32LocalGetFar:
	case I32LocalSetFar:
	case I32LocalTeeFar:
	case I64LocalGetFar:
	case I64LocalSetFar:
	case I64LocalTeeFar:
		return BA::SingleU32;
	case I32LocalGetNear:
	case I32LocalSetNear:
	case I32LocalTeeNear:
	case I64LocalGetNear:
	case I64LocalSetNear:
	case I64LocalTeeNear:
		return BA::SingleU8;
	case I32GlobalGet:
	case I32GlobalSet:
	case I64GlobalGet:
	case I64GlobalSet:
		return BA::SingleU64;
	case TableGet:
	case TableSet:
	case ElementDrop:
	case TableGrow:
	case TableSize:
	case TableFill:
		return BA::SingleU32;
	case TableCopy:
	case TableInit:
		return BA::DualU32;
	case I32LoadNear:
	case I64LoadNear:
	case I32StoreNear:
	case I64StoreNear:
		return BA::SingleU8;
	case I32LoadFar:
	case I64LoadFar:
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
	case I32StoreFar:
	case I64StoreFar:
	case I32Store8:
	case I32Store16:
	case I64Store8:
	case I64Store16:
	case I64Store32:
		return BA::SingleU32;
	case MemorySize:
	case MemoryGrow:
	case DataDrop:
	case MemoryFill:
	case MemoryCopy:
		return BA::None;
	case MemoryInit: return BA::DualU64;
	case I32Const: return BA::SingleU32;
	case I64Const: return BA::SingleU64;
	case F32Const: return BA::SingleU32;
	case F64Const: return BA::SingleU64;
	case I32EqualZero:
	case I32Equal:
	case I32NotEqual:
	case I32LesserS:
	case I32LesserU:
	case I32GreaterS:
	case I32GreaterU:
	case I32LesserEqualS:
	case I32LesserEqualU:
	case I32GreaterEqualS:
	case I32GreaterEqualU:
	case I64EqualZero:
	case I64Equal:
	case I64NotEqual:
	case I64LesserS:
	case I64LesserU:
	case I64GreaterS:
	case I64GreaterU:
	case I64LesserEqualS:
	case I64LesserEqualU:
	case I64GreaterEqualS:
	case I64GreaterEqualU:
	case F32Equal:
	case F32NotEqual:
	case F32Lesser:
	case F32Greater:
	case F32LesserEqual:
	case F32GreaterEqual:
	case F64Equal:
	case F64NotEqual:
	case F64Lesser:
	case F64Greater:
	case F64LesserEqual:
	case F64GreaterEqual:
	case I32CountLeadingZeros:
	case I32CountTrailingZeros:
	case I32CountOnes:
	case I32Add:
	case I32Subtract:
	case I32Multiply:
	case I32DivideS:
	case I32DivideU:
	case I32RemainderS:
	case I32RemainderU:
	case I32And:
	case I32Or:
	case I32Xor:
	case I32ShiftLeft:
	case I32ShiftRightS:
	case I32ShiftRightU:
	case I32RotateLeft:
	case I32RotateRight:
	case I64CountLeadingZeros:
	case I64CountTrailingZeros:
	case I64CountOnes:
	case I64Add:
	case I64Subtract:
	case I64Multiply:
	case I64DivideS:
	case I64DivideU:
	case I64RemainderS:
	case I64RemainderU:
	case I64And:
	case I64Or:
	case I64Xor:
	case I64ShiftLeft:
	case I64ShiftRightS:
	case I64ShiftRightU:
	case I64RotateLeft:
	case I64RotateRight:
	case F32Absolute:
	case F32Negate:
	case F32Ceil:
	case F32Floor:
	case F32Truncate:
	case F32Nearest:
	case F32SquareRoot:
	case F32Add:
	case F32Subtract:
	case F32Multiply:
	case F32Divide:
	case F32Minimum:
	case F32Maximum:
	case F32CopySign:
	case F64Absolute:
	case F64Negate:
	case F64Ceil:
	case F64Floor:
	case F64Truncate:
	case F64Nearest:
	case F64SquareRoot:
	case F64Add:
	case F64Subtract:
	case F64Multiply:
	case F64Divide:
	case F64Minimum:
	case F64Maximum:
	case F64CopySign:
	case I32WrapI64:
	case I32TruncateF32S:
	case I32TruncateF32U:
	case I32TruncateF64S:
	case I32TruncateF64U:
	case I64ExtendI32S:
	case I64ExtendI32U:
	case I64TruncateF32S:
	case I64TruncateF32U:
	case I64TruncateF64S:
	case I64TruncateF64U:
	case F32ConvertI32S:
	case F32ConvertI32U:
	case F32ConvertI64S:
	case F32ConvertI64U:
	case F32DemoteF64:
	case F64ConvertI32S:
	case F64ConvertI32U:
	case F64ConvertI64S:
	case F64ConvertI64U:
	case F64PromoteF32:
	case I32ReinterpretF32:
	case I64ReinterpretF64:
	case F32ReinterpretI32:
	case F64ReinterpretI64:
	case I32Extend8s:
	case I32Extend16s:
	case I64Extend8s:
	case I64Extend16s:
	case I64Extend32s:
	case I32TruncateSaturateF32S:
	case I32TruncateSaturateF32U:
	case I32TruncateSaturateF64S:
	case I32TruncateSaturateF64U:
	case I64TruncateSaturateF32S:
	case I64TruncateSaturateF32U:
	case I64TruncateSaturateF64S:
	case I64TruncateSaturateF64U:
		return BA::None;
	default: return BA::None;
	}
}

u32 BytecodeArguments::count() const {
	switch (value) {
		case None: return 0;
		case SingleU8: return 1;
		case SingleU32: return 1;
		case DualU32: return 2;
		case TripleU32: return 3;
		case SingleU64: return 1;
		case SingleU64SingleU32: return 1;
		case DualU64: return 2;
		default: assert(false);
	}
}

bool BytecodeArguments::isU8() const
{
	return value == SingleU8;
}

bool BytecodeArguments::isU32() const
{
	return value == SingleU32 || value == DualU32 || value == TripleU32 || value == SingleU64SingleU32;
}

bool BytecodeArguments::isU64() const
{
	return value == SingleU64 || value == SingleU64SingleU32 || value == DualU64;
}

u32 WASM::BytecodeArguments::sizeInBytes() const
{
	switch (value) {
		case None: return 0;
		case SingleU8: return 1;
		case SingleU32: return 4;
		case DualU32: return 8;
		case TripleU32: return 12;
		case SingleU64: return 8;
		case SingleU64SingleU32: return 12;
		case DualU64: return 16;
		default: assert(false);
	}
}

const char* ImportType::name() const
{
	switch (value) {
		case FunctionImport: return "FunctionImport";
		case TableImport: return "TableImport";
		case MemoryImport: return "MemoryImport";
		case GlobalImport: return "GlobalImport";
		default: return "<unknown import type>";
	}
}
