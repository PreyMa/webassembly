#pragma once

namespace WASM {
	class Buffer;
	class BufferSlice;
	class BufferIterator;

	template<typename, int> struct TypedIndex;

	struct ExportItem;
	struct CompressedLocalTypes;
	class Expression;
	class FunctionType;
	class Limits;
	class TableType;
	class MemoryType;
	class GlobalType;
	class DeclaredGlobalBase;
	class DeclaredGlobal;
	class Export;
	class Element;
	class DataItem;
	class FunctionCode;

	class ParsingState;
	class ModuleParser;
	class ModuleValidator;

	class SectionType;
	class ValType;
	class ExportType;
	class ElementMode;
	class DataItemMode;
	class NameSubsectionType;
	class BlockType;

	class Error;
	class ParsingError;
	class ValidationError;
	class LinkError;
	class CompileError;
	class LookupError;

	class InstructionType;
	class Instruction;

	struct BlockTypeIndexBase;
	struct BlockTypeIndex;
	struct BlockTypeResults;

	class Value;
	class ValuePack;
	class FunctionHandle;
	class Interpreter;

	class Function;
	class BytecodeFunction;
	class FunctionTable;
	class LinkedElement;
	class LinkedDataItem;
	class Memory;
	class GlobalBase;
	template<typename> class Global;
	struct ResolvedGlobal;
	class ModuleBase;
	class Module;

	class HostFunctionBase;
	template<typename> class HostFunction;

	struct NamedHostMemory;
	class HostGlobal;
	class HostMemory;
	class HostModuleBuilder;
	class HostModule;
	class HostModuleHandle;

	class Imported;
	class FunctionImport;
	class TableImport;
	class MemoryImport;
	class GlobalImport;

	class Bytecode;
	class BytecodeArguments;

	class Introspector;
	class DebugLogger;

	class ModuleLinker;
	class ModuleCompiler;

	template<typename> class Nullable;
	template<typename> class NoNull;

	template<typename> class ArrayList;
	template<typename> class SealedVector;
	template<typename, typename> class SealedUnorderedMap;
	template<typename> class SealedOptional;

	template<typename> class VirtualSpan;
	template<typename T> class VirtualForwardIterator;
	template<typename T> class TypedVirtualForwardIterator;
}
