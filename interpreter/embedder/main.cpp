
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

		using WASM::u32, WASM::i64, WASM::f32, WASM::f64, WASM::i32;
		WASM::HostModuleBuilder envModuleBuilder{ "env" };
		envModuleBuilder
			.defineFunction("abort", [&](u32, u32, u32, u32) { std::cout << "Abort called\n"; })
			.defineFunction("printInt", [&](i64 val) { std::cout << "printInt: " << val << std::endl; })
			.defineFunction("printFloat", [&](f64 val) { std::cout << "printFloat: " << val << std::endl; })
			.defineFunction("vecSum", [&](f32 a, f32 b, f32 c) { return a + b + c; })
			.defineMemory("memory", 1024)
			.defineGlobal("myGlobal", WASM::ValType::I32);

		interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/helloworld/build/debug.wasm");
		//interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/helloworld/build/release.wasm");
		//interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/ying/build/ying.debug.wasm");
		//interpreter.loadModule("C:/Users/Matthias/Documents/Uni/ABM/webassembly/webassembly/assemblyscript/yang/build/yang.debug.wasm");
		
		auto envModule= interpreter.registerHostModule(envModuleBuilder);
		interpreter.compileAndLinkModules();

		interpreter.runStartFunctions();

		//auto aFunction= interpreter.functionByName("debug", "fptest");
		//auto result= interpreter.runFunction(aFunction, (f32)5);
		// result.print(std::cout);

		auto memory= envModule.hostMemoryByName("memory");
		// memory->memoryView<WASM::u16>();

		auto global = envModule.hostGlobalByName("myGlobal");
	}
	catch (WASM::Error& e) {
		std::cerr << "\n\n========================================\n" << std::endl;
		std::cerr << "Caught wasm error: " << e << std::endl;
	}
	catch (std::exception& e) {
		std::cerr << "\n\n========================================\n" << std::endl;
		std::cerr << "Caught generic error: " << e.what() << std::endl;
	}

}
