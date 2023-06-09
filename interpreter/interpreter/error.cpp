#include <ostream>

#include "error.h"

using namespace WASM;

void ParsingError::print(std::ostream& o) const
{
	o << "Parsing error in '" << fileName << "' @" << std::hex << bytePosition << std::dec << ": " << message;
}

std::ostream& operator<<(std::ostream& out, const Error& e)
{
	e.print(out);
	return out;
}

void ValidationError::print(std::ostream& o) const
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

void WASM::LinkError::print(std::ostream& o) const
{
	o << "Link error in '";
	if (moduleName.size() <= 20) {
		o << moduleName << "'";
	}
	else {
		o << "..." << moduleName.substr(moduleName.size() - 17) << "'";
	}

	o << " while linking to '" << importItemName << "': " << message;
}

void WASM::LookupError::print(std::ostream& o) const
{
	if (!itemName.has_value()) {
		o << "Lookup error for module '" << moduleName << "': " << message;
		return;
	}

	o << "Lookup error in module '" << moduleName << "' for item " << "'" << *itemName << "': " << message;
}
