#pragma once

#include<exception>
#include<string>

#include "util.h"

namespace WASM {

	class Error : public std::exception {
	public:
		Error(std::string  m) : message{ std::move(m) } {}
		virtual ~Error() = default;

		virtual void print(std::ostream&) const= 0;

	protected:
		std::string message;
	};

	class ParsingError : public Error {
	public:
		ParsingError(u64 b, std::string f, std::string m)
			: Error{ std::move(m) }, bytePosition{ b }, fileName{ std::move(f) } {}

		virtual const char* what() const override { return message.c_str(); }
		virtual void print(std::ostream& o) const override;

	private:
		u64 bytePosition;
		std::string fileName;
	};

	class ValidationError : public Error {

	};

	std::ostream& operator<<(std::ostream&, const Error&);
}
