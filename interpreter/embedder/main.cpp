
#include <iostream>

#include "../interpreter/interpreter.h"
#include "../interpreter/introspection.h"
#include "../interpreter/error.h"


int main() {
	std::cout << "hello world\n";

	try {
		WASM::Interpreter interpreter;

		auto logger = std::make_unique<WASM::ConsoleLogger>( std::cout );
		interpreter.attachIntrospector(std::move(logger));

		using WASM::u32, WASM::i64;
		WASM::HostModuleBuilder envModule{ "env" };
		envModule
			.defineFunction("abort", [&](u32, u32, u32, u32) { std::cout << "Abort called"; })
			.defineFunction("printInt", [&](i64 val) { std::cout << "printInt: " << val; });

		interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/helloworld/build/debug.wasm");
		//interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/ying/build/ying.debug.wasm");
		//interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/yang/build/yang.debug.wasm");
		interpreter.registerHostModule(envModule.toModule());
		interpreter.compileAndLinkModules();

		interpreter.runStartFunctions();
	}
	catch (WASM::Error& e) {
		std::cerr << "\n\n========================================\n" << std::endl;
		std::cerr << "Caught wasm error: " << e << std::endl;
	}

}
