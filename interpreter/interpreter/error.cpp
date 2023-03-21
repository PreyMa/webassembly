#include <ostream>

#include "error.h"

using namespace WASM;

void ParsingError::print(std::ostream& o) const
{
	o << "Parsing error in '" << fileName << "' @" << std::hex << bytePosition << std::dec << ": " << message;
}

std::ostream& WASM::operator<<(std::ostream& out, const Error& e)
{
	e.print(out);
	return out;
}

void WASM::ValidationError::print(std::ostream& o) const
{
	o << "Validation error in '" << fileName << "': " << message;
}

void CompileError::print(std::ostream& o) const
{
	o << "Compilation error in '";
	if (moduleName.size() <= 20) {
		o << moduleName << "'";
	}
	else {
		o << "..." << moduleName.substr(moduleName.size() - 17) << "'";
	}
	if (functionIndex.has_value()) {
		o << " while compiling function " << *functionIndex;
	}

	o << ": " << message;
}
