#pragma once

#include "module.h"

namespace WASM {
	class Interpreter {
	public:
		void loadModule(std::string);
		void compileAndLinkModules();

	private:
		friend class ModuleCompiler;

		Nullable<Function> findFunction(const std::string&, const std::string&);

		std::vector<Module> modules;
		std::unordered_map<std::string, Nullable<Module>> moduleNameMap;

		bool hasLinked{ false };
	};
}