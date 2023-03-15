
#include "interpreter.h"
#include "module.h"

#include <iostream>

using namespace WASM;

void Interpreter::loadModule(std::string path)
{
	auto buffer= Buffer::fromFile(path);
	ModuleParser parser;
	auto module= parser.parse(std::move(buffer), std::move(path));
}
