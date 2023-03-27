#pragma once

#include "host_function.h"

namespace WASM {
	class Interpreter {
	public:
		Interpreter();
		~Interpreter();

		void loadModule(std::string);
		void compileAndLinkModules();

		void attachIntrospector(std::unique_ptr<Introspector>);

	private:
		friend class ModuleCompiler;

		Nullable<Function> findFunction(const std::string&, const std::string&);

		std::vector<Module> modules;
		std::unordered_map<std::string, Nullable<Module>> moduleNameMap;

		bool hasLinked{ false };
		std::unique_ptr<Introspector> attachedIntrospector;
	};
}