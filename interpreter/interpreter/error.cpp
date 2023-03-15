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
