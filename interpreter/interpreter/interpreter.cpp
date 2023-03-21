
#include "interpreter.h"
#include "bytecode.h"

#include <iostream>

using namespace WASM;

void Interpreter::loadModule(std::string path)
{
	if (hasLinked) {
		throw std::runtime_error{ "Cannot load module after linking step" };
	}

	auto buffer= Buffer::fromFile(path);
	ModuleParser parser;
	parser.parse(std::move(buffer), std::move(path));

	ModuleValidator validator;
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

	// TODO: Linking

	for (auto& module : modules) {
		ModuleCompiler compiler{ *this, module };
		compiler.compile();
	}

	hasLinked = true;
}

Nullable<Function> Interpreter::findFunction(const std::string& moduleName, const std::string& functionName)
{
	auto moduleFind = moduleNameMap.find(moduleName);
	if (moduleFind == moduleNameMap.end()) {
		return {};
	}

	return moduleFind->second->exportedFunctionByName(functionName);
}
