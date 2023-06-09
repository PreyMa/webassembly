#pragma once

#include <string_view>
#include <ostream>
#include <span>
#include <unordered_map>

#include "util.h"
#include "forward.h"
#include "indices.h"

namespace WASM {
	class Introspector {
	public:
		using NameMap = std::unordered_map<u32, std::string>;
		using IndirectNameMap = std::unordered_map<u32, NameMap>;

		virtual void onModuleParsingStart(std::string_view) = 0;
		virtual void onModuleParsingFinished(std::span<FunctionCode>) = 0;
		virtual void onSkippingUnrecognizedSection(SectionType, sizeType)= 0;
		virtual void onParsingCustomSection(std::string_view, const BufferSlice&)= 0;
		virtual void onParsingNameSection(std::string_view, const NameMap&, const IndirectNameMap&)= 0;
		virtual void onSkippingUnrecognizedNameSubsection(NameSubsectionType, sizeType)= 0;
		virtual void onParsingTypeSection(std::span<FunctionType>)= 0;
		virtual void onParsingFunctionSection(std::span<ModuleTypeIndex>)= 0;
		virtual void onParsingTableSection(std::span<TableType>) = 0;
		virtual void onParsingMemorySection(std::span<MemoryType>) = 0;
		virtual void onParsingGlobalSection(std::span<DeclaredGlobal>) = 0;
		virtual void onParsingExportSection(std::span<Export>) = 0;
		virtual void onParsingStrartSection(ModuleFunctionIndex) = 0;
		virtual void onParsingElementSection(std::span<Element>) = 0;
		virtual void onParsingCodeSection(std::span<FunctionCode>) = 0;
		virtual void onParsingImportSection(std::span<FunctionImport>, std::span<TableImport>, std::span<MemoryImport>, std::span<GlobalImport>) = 0;
		virtual void onParsingDataCountSection(u32) = 0;
		virtual void onParsingDataSection(std::span<DataItem>) = 0;

		virtual void onModuleValidationStart() = 0;
		virtual void onModuleValidationFinished() = 0;
		virtual void onModulImportsValidationStart() = 0;
		virtual void onModulImportsValidationFinished() = 0;
		virtual void onValidatingFunction(ModuleFunctionIndex, const FunctionType&) = 0;
		virtual void onValidatingTableType(const TableType&) = 0;
		virtual void onValidatingMemoryType(const MemoryType&) = 0;
		virtual void onValidatingExport(const Export&) = 0;
		virtual void onValidatingStartFunction(ModuleFunctionIndex) = 0;
		virtual void onValidatingGlobal(const DeclaredGlobal&) = 0;
		virtual void onValidatingElement(const Element&) = 0;
		virtual void onValidatingDataItem(const DataItem&) = 0;

		virtual void onModuleLinkingStart() = 0;
		virtual void onModuleLinkingFinished() = 0;
		virtual void onAddingLinkingDependency(const Module& importingModule, const Imported& import, ModuleExportIndex idx) = 0;
		virtual void onLinkingDependencyResolved(const Module& importingModule, const Imported& import) = 0;

		virtual void onRegisteredModule(const ModuleBase&) = 0;
		virtual void onModuleTableInitialized(const Module&, sizeType, sizeType) = 0;
		virtual void onModuleMemoryInitialized(const Module&, sizeType, sizeType) = 0;

		virtual void onCompiledFunction(const Module&, const BytecodeFunction&)= 0;
	};

	class DebugLogger : public Introspector {
	public:
		virtual void onModuleParsingStart(std::string_view) override;
		virtual void onModuleParsingFinished(std::span<FunctionCode>) override;
		virtual void onSkippingUnrecognizedSection(SectionType, sizeType) override;
		virtual void onParsingCustomSection(std::string_view, const BufferSlice&) override;
		virtual void onParsingNameSection(std::string_view, const NameMap&, const IndirectNameMap&) override;
		virtual void onSkippingUnrecognizedNameSubsection(NameSubsectionType, sizeType) override;
		virtual void onParsingTypeSection(std::span<FunctionType>) override;
		virtual void onParsingFunctionSection(std::span<ModuleTypeIndex>) override;
		virtual void onParsingTableSection(std::span<TableType>) override;
		virtual void onParsingMemorySection(std::span<MemoryType>) override;
		virtual void onParsingGlobalSection(std::span<DeclaredGlobal>) override;
		virtual void onParsingExportSection(std::span<Export>) override;
		virtual void onParsingStrartSection(ModuleFunctionIndex) override;
		virtual void onParsingElementSection(std::span<Element>) override;
		virtual void onParsingCodeSection(std::span<FunctionCode>) override;
		virtual void onParsingImportSection(std::span<FunctionImport>, std::span<TableImport>, std::span<MemoryImport>, std::span<GlobalImport>) override;
		virtual void onParsingDataCountSection(u32) override;
		virtual void onParsingDataSection(std::span<DataItem>) override;

		virtual void onModuleValidationStart() override;
		virtual void onModuleValidationFinished() override;
		virtual void onModulImportsValidationStart() override;
		virtual void onModulImportsValidationFinished() override;
		virtual void onValidatingFunction(ModuleFunctionIndex, const FunctionType&) override;
		virtual void onValidatingTableType(const TableType&) override;
		virtual void onValidatingMemoryType(const MemoryType&) override;
		virtual void onValidatingExport(const Export&) override;
		virtual void onValidatingStartFunction(ModuleFunctionIndex) override;
		virtual void onValidatingGlobal(const DeclaredGlobal&) override;
		virtual void onValidatingElement(const Element&) override;
		virtual void onValidatingDataItem(const DataItem&) override;

		virtual void onModuleLinkingStart() override;
		virtual void onModuleLinkingFinished() override;
		virtual void onAddingLinkingDependency(const Module& importingModule, const Imported& import, ModuleExportIndex idx) override;
		virtual void onLinkingDependencyResolved(const Module& importingModule, const Imported& import) override;

		virtual void onRegisteredModule(const ModuleBase&) override;
		virtual void onModuleTableInitialized(const Module&, sizeType, sizeType) override;
		virtual void onModuleMemoryInitialized(const Module&, sizeType, sizeType) override;

		virtual void onCompiledFunction(const Module&, const BytecodeFunction&) override;

	protected:
		virtual std::ostream& outStream() = 0;
		virtual bool doLoggingWhenParsing() = 0;
		virtual bool doLoggingWhenValidating() = 0;
		virtual bool doLoggingWhenLinking() = 0;
		virtual bool doLoggingWhenCompiling() = 0;

	private:
		bool isValidatingImports{ false };
	};

	class ConsoleLogger : public DebugLogger {
	public:
		ConsoleLogger(std::ostream& s, bool lp= true, bool lv= true, bool ll= true, bool lc= true)
			: stream{ s }, logWhenParsing{ lp }, logWhenValidating{ lv }, logWhenLinking{ ll }, logWhenCompiling{ lc } {}

	protected:
		virtual std::ostream& outStream() override;
		virtual bool doLoggingWhenParsing() override;
		virtual bool doLoggingWhenValidating() override;
		virtual bool doLoggingWhenLinking() override;
		virtual bool doLoggingWhenCompiling() override;

		bool logWhenParsing;
		bool logWhenValidating;
		bool logWhenLinking;
		bool logWhenCompiling;
		std::ostream& stream;
	};
}
