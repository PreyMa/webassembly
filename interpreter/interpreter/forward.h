#pragma once

namespace WASM {
	class Buffer;
	class BufferSlice;
	class BufferIterator;

	struct ExportItem;
	struct CompressedLocalTypes;
	class Expression;
	class FunctionType;
	class Limits;
	class TableType;
	class MemoryType;
	class GlobalType;
	class DeclaredGlobal;
	class Export;
	class Element;
	class FunctionCode;

	class ParsingState;
	class ModuleParser;
	class ModuleValidator;

	class SectionType;
	class ValType;
	class ExportType;
	class ElementMode;
	class NameSubsectionType;
	class BlockType;

	class Error;
	class ParsingError;
	class ValidationError;

	class InstructionType;
	class Instruction;

	struct BlockTypeIndexBase;
	struct BlockTypeIndex;
	struct BlockTypeResults;

	class Interpreter;

	class Function;
	class BytecodeFunction;
	class FunctionTable;
	class DecodedElement;
	class Memory;
	class GlobalBase;
	template<typename> class Global;
	class Module;

	class HostFunctionBase;
	template<typename> class HostFunction;

	struct Imported;
	struct FunctionImport;
	struct TableImport;
	struct MemoryImport;
	struct GlobalImport;

	class Bytecode;
	class BytecodeArguments;

	class ModuleLinker;
	class ModuleCompiler;

	template<typename> class ArrayList;
}
