
#include <iostream>

#include "../interpreter/interpreter.h"
#include "../interpreter/error.h"

int main() {
	std::cout << "hello world\n";

	try {
		WASM::Interpreter interpreter;
		interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/helloworld/build/debug.wasm");
	}
	catch (WASM::Error& e) {
		std::cerr << "\n\n========================================\n" << std::endl;
		std::cerr << "Caught wasm error: " << e << std::endl;
	}

}
