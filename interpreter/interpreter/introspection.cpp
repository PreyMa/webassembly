
#include "introspection.h"
#include "module.h"

using namespace WASM;

void DebugLogger::onModuleParsingStart(std::string_view modulePath)
{
	auto& stream= outStream();
	stream << "Parsing module: '";
	
	if (modulePath.size() > 40) {
		stream << "..." << modulePath.substr(modulePath.size()- 40, 40);
	}
	else {
		stream << modulePath;
	}

	stream << "'" << std::endl;
}

void DebugLogger::onModuleParsingFinished(std::span<FunctionCode> functionCodes)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		for (auto& f : functionCodes) {
			stream << "=> Function:";
			f.printBody(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onSkippingUnrecognizedSection(SectionType type, sizeType numBytes)
{
	if (doLoggingWhenParsing()) {
		outStream() << "Section type not recognized '" << type.name() << "'. skipping " << numBytes << " bytes" << std::endl;
	}
}

void DebugLogger::onParsingCustomSection(std::string_view name, const BufferSlice& dataSlice)
{
	if (doLoggingWhenParsing()) {
		outStream() << "-> Parsed custom section '" << name << "' containing " << dataSlice.size() << " bytes" << std::endl;
	}
}

void DebugLogger::onParsingNameSection(std::string_view moduleName, const NameMap& functionNames, const IndirectNameMap& functionLocalNames)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed custom section 'name'" << std::endl;

		if (!moduleName.empty()) {
			stream << "  - Module name: " << moduleName << std::endl;
		}

		if (!functionNames.empty()) {
			stream << "  - Function names: " << std::endl;
			for (auto& n : functionNames) {
				stream << "    - " << n.first << " -> " << n.second << std::endl;
			}
		}

		if (!functionLocalNames.empty()) {
			stream << "  - Local names: " << std::endl;
			for (auto& g : functionLocalNames) {
				stream << "    - Group: " << g.first << std::endl;
				for (auto& n : g.second) {
					stream << "      - " << n.first << " -> " << n.second << std::endl;
				}
			}
		}
	}
}

void WASM::DebugLogger::onSkippingUnrecognizedNameSubsection(NameSubsectionType type, sizeType numBytes)
{
	if (doLoggingWhenParsing()) {
		outStream() << "  Name subsection type not recognized '" << type.name() << "'. skipping " << numBytes << " bytes" << std::endl;
	}
}

void DebugLogger::onParsingTypeSection(std::span<FunctionType> functionTypes)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed type section containing " << functionTypes.size() << " function types" << std::endl;

		u32 i = 0;
		for (auto& functionType : functionTypes) {
			stream << "  - " << i++ << " ";
			functionType.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onParsingFunctionSection(std::span<ModuleTypeIndex> functionDeclarations)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed function section containing " << functionDeclarations.size() << " functions" << std::endl;

		u32 i = 0;
		for (auto typeIdx : functionDeclarations) {
			stream << "  - " << i++ << " type id: " << typeIdx << std::endl;
		}
	}
}

void DebugLogger::onParsingTableSection(std::span<TableType> tableTypes)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed table section containing " << tableTypes.size() << " tables" << std::endl;

		u32 i = 0;
		for (auto& tableType : tableTypes) {
			stream << "  - " << i++ << " ";
			tableType.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onParsingMemorySection(std::span<MemoryType> memoryTypes)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed memory section containing " << memoryTypes.size() << " memories" << std::endl;

		u32 i = 0;
		for (auto& memoryType : memoryTypes) {
			stream << "  - " << i++ << " ";
			memoryType.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onParsingGlobalSection(std::span<DeclaredGlobal> declaredGlobals)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed global section containing " << declaredGlobals.size() << " globals" << std::endl;

		u32 i = 0;
		for (auto& declaredGlobal : declaredGlobals) {
			stream << "  - " << i++ << " ";
			declaredGlobal.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onParsingExportSection(std::span<Export> exports)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed export section containing " << exports.size() << " exports" << std::endl;

		u32 i = 0;
		for (auto& exportItem : exports) {
			stream << "  - " << i++ << " ";
			exportItem.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onParsingStrartSection(ModuleFunctionIndex startFunctionIndex)
{
	if (doLoggingWhenParsing()) {
		outStream() << "-> Parsed start section containing start function index " << startFunctionIndex << std::endl;
	}
}

void DebugLogger::onParsingElementSection(std::span<Element> elements)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed element section containing " << elements.size() << " elements" << std::endl;

		u32 i = 0;
		for (auto& element : elements) {
			stream << "  - " << i++ << " ";
			element.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onParsingCodeSection(std::span<FunctionCode> functionCodes)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed code section containing " << functionCodes.size() << " function code items" << std::endl;

		u32 i = 0;
		for (auto& code : functionCodes) {
			stream << "  - " << i++ << " ";
			code.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onParsingImportSection(
	std::span<FunctionImport> functionImports,
	std::span<TableImport> tableImports,
	std::span<MemoryImport> memoryImports,
	std::span<GlobalImport> globalImports
)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();

		auto numImports = functionImports.size() + tableImports.size() + memoryImports.size() + globalImports.size();
		stream << "-> Parsed import section containing " << numImports << " import items" << std::endl;

		auto printImport = [&](ImportType type, const Imported& imported) {
			stream << "  - " << type.name() << ": " << imported.module() << " :: " << imported.name();
		};

		u32 i = 0;
		for (auto& funcImport : functionImports) {
			printImport(ImportType::FunctionImport, funcImport);
			stream << " (id: " << i++ << ") indexing type: " << funcImport.moduleTypeIndex() << std::endl;
		}

		for (auto& tableImport : tableImports) {
			printImport(ImportType::TableImport, tableImport);
			stream << " type: ";
			tableImport.tableType().print(stream);
			stream << std::endl;
		}

		for (auto& memImport : memoryImports) {
			printImport(ImportType::MemoryImport, memImport);
			stream << " type: ";
			memImport.memoryType().print(stream);
			stream << std::endl;
		}

		for (auto& globalImport : globalImports) {
			printImport(ImportType::GlobalImport, globalImport);
			stream << " type: " << globalImport.globalType().isMutable() ? "mutable" : "constant";
			stream << " " << globalImport.globalType().valType().name() << std::endl;
		}
	}
}

void WASM::DebugLogger::onParsingDataCountSection(u32 count)
{
	if (doLoggingWhenParsing()) {
		outStream() << "-> Parsed data count section expecting " << count << " data sections in this module" << std::endl;
	}
}

void WASM::DebugLogger::onParsingDataSection(std::span<DataItem> dataItems)
{
	if (doLoggingWhenParsing()) {
		auto& stream = outStream();
		stream << "-> Parsed data section containing " << dataItems.size() << " data items" << std::endl;

		u32 i = 0;
		for (auto& item : dataItems) {
			stream << "  - " << i++ << " ";
			item.print(stream);
			stream << std::endl;
		}
	}
}

void DebugLogger::onModuleValidationStart()
{
	isValidatingImports = false;
}

void DebugLogger::onModuleValidationFinished()
{
	isValidatingImports = true;
}

void DebugLogger::onModulImportsValidationStart()
{
	isValidatingImports = false;
}

void DebugLogger::onModulImportsValidationFinished()
{
}

void DebugLogger::onValidatingFunction(ModuleFunctionIndex functionIdx, const FunctionType& functionType)
{
	if (doLoggingWhenValidating()) {
		auto& stream = outStream();
		stream << "Validated function " << functionIdx << " with type ";
		functionType.print(stream);
		stream << std::endl;
	}
}

void DebugLogger::onValidatingTableType(const TableType& tableType)
{
	if (doLoggingWhenValidating()) {
		auto& stream = outStream();
		if (isValidatingImports) {
			stream << "IMPORT: ";
		}
		stream << "Validated table type: ";
		tableType.print(stream);
		stream << std::endl;
	}
}

void DebugLogger::onValidatingMemoryType(const MemoryType& memoryType)
{
	if (doLoggingWhenValidating()) {
		auto& stream = outStream();
		if (isValidatingImports) {
			stream << "IMPORT: ";
		}
		stream << "Validated memory type: ";
		memoryType.print(stream);
		stream << std::endl;
	}
}

void DebugLogger::onValidatingExport(const Export& exportItem)
{
	if (doLoggingWhenValidating()) {
		outStream() << "Validated export '" << exportItem.name() << "'" << std::endl;
	}
}

void DebugLogger::onValidatingStartFunction(ModuleFunctionIndex functionIdx)
{
	if (doLoggingWhenValidating()) {
		outStream() << "Validated start function with index " << functionIdx << std::endl;
	}
}

void DebugLogger::onValidatingGlobal(const DeclaredGlobal& global)
{
	if (doLoggingWhenValidating()) {
		auto& stream = outStream();
		stream << "Validated global with type ";
		global.print(stream);
		stream << std::endl;
	}
}

void DebugLogger::onValidatingElement(const Element& element)
{
	if (doLoggingWhenValidating()) {
		auto& stream = outStream();
		stream << "Validated element segment ";
		element.print(stream);
		stream << std::endl;
	}
}

void WASM::DebugLogger::onValidatingDataItem(const DataItem& item)
{
	if (doLoggingWhenValidating()) {
		auto& stream = outStream();
		stream << "Validated data item";
		item.print(stream, false);
		stream << std::endl;
	}
}

void WASM::DebugLogger::onModuleLinkingStart()
{
}

void WASM::DebugLogger::onModuleLinkingFinished()
{
}

void WASM::DebugLogger::onAddingLinkingDependency(const Module& importingModule, const Imported& import, ModuleExportIndex idx)
{
	if (doLoggingWhenLinking()) {
		auto& stream = outStream();
		stream << "Created dependency item for module '" << importingModule.name() << "': ";
		stream << import.module() << "::" << import.name();
		stream << " (type: " << import.requiredExportType().name() << " idx: " << idx << ")" << std::endl;
	}
}

void WASM::DebugLogger::onLinkingDependencyResolved(const Module& importingModule, const Imported& import)
{
	if (doLoggingWhenLinking()) {
		auto& stream = outStream();
		stream << "- Resolved dependency from module '" << importingModule.name() << "': " << import.module() << "::" << import.name() << std::endl;
	}
}

void WASM::DebugLogger::onRegisteredModule(const ModuleBase& module)
{
	auto type = module.asHostModule().has_value() ? "host module" : "wasm module";
	outStream() << "Registerd " << type << ": " << module.name() << std::endl;
}

void WASM::DebugLogger::onModuleTableInitialized(const Module& module, sizeType numElements, sizeType numFunctions)
{
	outStream() << "Initalized tables in module '" << module.name() << "'. " << numFunctions <<
		" function entries initialized by " << numElements << " active element segments" << std::endl;
}

void WASM::DebugLogger::onModuleMemoryInitialized(const Module& module, sizeType numDataItems, sizeType numBytes)
{
	outStream() << "Initalized memory instance in module '" << module.name() << "'. " << numBytes <<
		" bytes initialized by " << numDataItems << " active data segment items" << std::endl;
}

void WASM::DebugLogger::onCompiledFunction(const Module& module, const BytecodeFunction& function)
{
	if (doLoggingWhenCompiling()) {
		std::string modName{ module.name() };
		if (modName.size() > 20) {
			modName = "..." + modName.substr(modName.size() - 17);
		}

		auto maybeFunctionName = function.lookupName(module);
		auto* functionName = maybeFunctionName.has_value() ? maybeFunctionName->c_str() : "<unknown name>";

		auto& stream = outStream();
		stream << "Compiled function " << modName << " :: " << functionName << " (index " << function.moduleIndex() << ") ";
		stream << "(max stack height " << function.maxStackHeight() / 4 << " slots)" << std::endl;

		ModuleCompiler::printBytecode(stream, function.bytecode());
	}
}

std::ostream& WASM::ConsoleLogger::outStream()
{
	return stream;
}

bool WASM::ConsoleLogger::doLoggingWhenParsing()
{
	return logWhenParsing;
}

bool WASM::ConsoleLogger::doLoggingWhenValidating()
{
	return logWhenValidating;
}

bool WASM::ConsoleLogger::doLoggingWhenLinking()
{
	return logWhenLinking;
}

bool WASM::ConsoleLogger::doLoggingWhenCompiling()
{
	return logWhenCompiling;
}
