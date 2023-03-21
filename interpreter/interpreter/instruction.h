#pragma once

#include <optional>

#include "util.h"
#include "enum.h"
#include "buffer.h"

namespace WASM {
	class InstructionType : public Enum <InstructionType> {
	public:
		enum TEnum {
			Unreachable,
			NoOperation,
			Block,
			Loop,
			If,
			Else,
			End,
			Branch,
			BranchIf,
			BranchTable,
			Return,
			Call,
			CallIndirect,
			Drop,
			Select,
			SelectFrom,
			LocalGet,
			LocalSet,
			LocalTee,
			GlobalGet,
			GlobalSet,
			ReferenceNull,
			ReferenceIsNull,
			ReferenceFunction,
			TableGet,
			TableSet,
			TableInit,
			ElementDrop,
			TableCopy,
			TableGrow,
			TableSize,
			TableFill,
			I32Load,
			I64Load,
			F32Load,
			F64Load,
			I32Load8s,
			I32Load8u,
			I32Load16s,
			I32Load16u,
			I64Load8s,
			I64Load8u,
			I64Load16s,
			I64Load16u,
			I64Load32s,
			I64Load32u,
			I32Store,
			I64Store,
			F32Store,
			F64Store,
			I32Store8,
			I32Store16,
			I64Store8,
			I64Store16,
			I64Store32,
			MemorySize,
			MemoryGrow,
			MemoryInit,
			DataDrop,
			MemoryCopy,
			MemoryFill,
			I32Const,
			I64Const,
			F32Const,
			F64Const,
			I32EqualZero,
			I32Equal,
			I32NotEqual,
			I32LesserS,
			I32LesserU,
			I32GreaterS,
			I32GreaterU,
			I32LesserEqualS,
			I32LesserEqualU,
			I32GreaterEqualS,
			I32GreaterEqualU,
			I64EqualZero,
			I64Equal,
			I64NotEqual,
			I64LesserS,
			I64LesserU,
			I64GreaterS,
			I64GreaterU,
			I64LesserEqualS,
			I64LesserEqualU,
			I64GreaterEqualS,
			I64GreaterEqualU,
			F32Equal,
			F32NotEqual,
			F32Lesser,
			F32Greater,
			F32LesserEqual,
			F32GreaterEqual,
			F64Equal,
			F64NotEqual,
			F64Lesser,
			F64Greater,
			F64LesserEqual,
			F64GreaterEqual,
			I32CountLeadingZeros,
			I32CountTrailingZeros,
			I32CountOnes,
			I32Add,
			I32Subtract,
			I32Multiply,
			I32DivideS,
			I32DivideU,
			I32RemainderS,
			I32RemainderU,
			I32And,
			I32Or,
			I32Xor,
			I32ShiftLeft,
			I32ShiftRightS,
			I32ShiftRightU,
			I32RotateLeft,
			I32RotateRight,
			I64CountLeadingZeros,
			I64CountTrailingZeros,
			I64CountOnes,
			I64Add,
			I64Subtract,
			I64Multiply,
			I64DivideS,
			I64DivideU,
			I64RemainderS,
			I64RemainderU,
			I64And,
			I64Or,
			I64Xor,
			I64ShiftLeft,
			I64ShiftRightS,
			I64ShiftRightU,
			I64RotateLeft,
			I64RotateRight,
			F32Absolute,
			F32Negate,
			F32Ceil,
			F32Floor,
			F32Truncate,
			F32Nearest,
			F32SquareRoot,
			F32Add,
			F32Subtract,
			F32Multiply,
			F32Divide,
			F32Minimum,
			F32Maximum,
			F32CopySign,
			F64Absolute,
			F64Negate,
			F64Ceil,
			F64Floor,
			F64Truncate,
			F64Nearest,
			F64SquareRoot,
			F64Add,
			F64Subtract,
			F64Multiply,
			F64Divide,
			F64Minimum,
			F64Maximum,
			F64CopySign,
			I32WrapI64,
			I32TruncateF32S,
			I32TruncateF32U,
			I32TruncateF64S,
			I32TruncateF64U,
			I64ExtendI32S,
			I64ExtendI32U,
			I64TruncateF32S,
			I64TruncateF32U,
			I64TruncateF64S,
			I64TruncateF64U,
			F32ConvertI32S,
			F32ConvertI32U,
			F32ConvertI64S,
			F32ConvertI64U,
			F32DemoteF64,
			F64ConvertI32S,
			F64ConvertI32U,
			F64ConvertI64S,
			F64ConvertI64U,
			F64PromoteF32,
			I32ReinterpretF32,
			I64ReinterpretF64,
			F32ReinterpretI32,
			F64ReinterpretI64,
			I32Extend8s,
			I32Extend16s,
			I64Extend8s,
			I64Extend16s,
			I64Extend32s,
			I32TruncateSaturateF32S,
			I32TruncateSaturateF32U,
			I32TruncateSaturateF64S,
			I32TruncateSaturateF64U,
			I64TruncateSaturateF32S,
			I64TruncateSaturateF32U,
			I64TruncateSaturateF64S,
			I64TruncateSaturateF64U,

			NumberOfItems
		};

		using Enum<InstructionType>::Enum;
		InstructionType(TEnum e) : Enum<InstructionType>{ e } {}
		
		static InstructionType fromWASMBytes(BufferIterator&);

		bool isConstant() const;
		bool isBinary() const;
		bool isUnary() const;
		bool isBlock() const;
		bool isMemory() const;
		bool requiresModuleInstance() const;
		std::optional<ValType> operandType() const;
		std::optional<ValType> resultType() const;
		std::optional<ValType> constantType() const;

		const char* name() const;
	};

	struct BlockTypeIndexBase {
		BlockType blockType;
		u32 index;

		bool operator==(BlockType t) const { return blockType == t; }
	};

	using BlockTypeParameters = std::optional<u32>;
	struct BlockTypeResults final : public BlockTypeIndexBase {};

	struct BlockTypeIndex final : public BlockTypeIndexBase {
		BlockTypeParameters parameters() const;
		BlockTypeResults results() const;
	};

	class Instruction {
	public:

		template<typename T>
		struct Constant { T x; };

		template<typename T> Constant(T) -> Constant<T>;

		Instruction(InstructionType t, u32 a= 0, u32 b= 0)
			: type{ t }, operandA{ a }, operandB{ b } {}

		Instruction(InstructionType t, Constant<i32> c)
			: type{ t }, i32Constant{ c.x } {};

		Instruction(InstructionType t, Constant<i64> c)
			: type{ t }, i64Constant{ c.x } {};

		Instruction(InstructionType t, Constant<f32> c)
			: type{ t }, f32Constant{ c.x } {};

		Instruction(InstructionType t, Constant<f64> c)
			: type{ t }, f64Constant{ c.x } {};

		Instruction(InstructionType t, const u8* p)
			: type{ t }, vectorPointer{ p } {};

		Instruction(InstructionType t, const u8* p, u32 c)
			: type{ t }, operandC{ c }, vectorPointer{ p } {};

		static Instruction fromWASMBytes(BufferIterator&);

		void print(std::ostream&, const BufferSlice&) const;
		bool isConstant() const { return type.isConstant(); }
		std::optional<ValType> constantType() const { return type.constantType(); }

		InstructionType opCode() const { return type; }
		bool operator==(InstructionType t) const { return type == t; }

		BlockTypeIndex blockTypeIndex() const;
		u32 branchLabel() const;
		u32 localIndex() const;
		u32 functionIndex() const;
		u32 memoryOffset() const;

		i32 asI32Constant() const;
		u32 asIF32Constant() const;
		u64 asIF64Constant() const;
		std::optional<u32> asReferenceIndex() const;

		std::optional<Bytecode> toBytecode() const;
		u32 maxPrintedByteLength(const BufferSlice&) const;

	private:
		static Instruction parseBlockTypeInstruction(InstructionType, BufferIterator&);
		static Instruction parseBranchTableInstruction(BufferIterator&);
		static Instruction parseSelectVectorInstruction(BufferIterator&);

		void printBlockTypeInstruction(std::ostream&) const;
		void printBranchTableInstruction(std::ostream&, const BufferSlice&) const;
		void printSelectVectorInstruction(std::ostream&, const BufferSlice&) const;

		InstructionType type;
		u32 operandC;

		union {
			struct {
				u32 operandA;
				u32 operandB;
			};
			i32 i32Constant;
			i64 i64Constant;
			f32 f32Constant;
			f64 f64Constant;
			const u8* vectorPointer;
		};
	};
}
