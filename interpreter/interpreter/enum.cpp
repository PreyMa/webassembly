#include "enum.h"

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
	case Global: return "Global";
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
