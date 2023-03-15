
#include <iostream>

#include "../interpreter/interpreter.h"

int main() {
	std::cout << "hello world\n";

	WASM::Interpreter interpreter;
	interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/helloworld/build/debug.wasm");

}
