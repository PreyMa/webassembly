
#include "interpreter.h"
#include "introspection.h"
#include "bytecode.h"

#include <iostream>

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
		ModuleLinker linker{ modules };
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

void Interpreter::attachIntrospector(std::unique_ptr<Introspector> introspector)
{
	attachedIntrospector = std::move(introspector);
}

Nullable<Function> Interpreter::findFunction(const std::string& moduleName, const std::string& functionName)
{
	auto moduleFind = moduleNameMap.find(moduleName);
	if (moduleFind == moduleNameMap.end()) {
		return {};
	}

	return moduleFind->second->exportedFunctionByName(functionName);
}
