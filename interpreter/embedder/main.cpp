
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

		interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/helloworld/build/debug.wasm");
		interpreter.compileAndLinkModules();

		interpreter.runStartFunctions();
	}
	catch (WASM::Error& e) {
		std::cerr << "\n\n========================================\n" << std::endl;
		std::cerr << "Caught wasm error: " << e << std::endl;
	}

}
