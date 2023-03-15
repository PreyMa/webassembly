// interpreter.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <string>

namespace WASM {
	class Interpreter {
	public:
		void loadModule(std::string);
	};
}