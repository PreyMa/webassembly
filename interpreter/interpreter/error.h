#pragma once

#include<exception>
#include<optional>
#include<string>

#include "util.h"
#include "indices.h"

namespace WASM {

	class Error : public std::exception {
	public:
		Error(std::string  m) : message{ std::move(m) } {}
		virtual ~Error() = default;

		virtual const char* what() const final { return message.c_str(); }
		virtual void print(std::ostream&) const= 0;

	protected:
		std::string message;
	};

	class ParsingError : public Error {
	public:
		ParsingError(u64 b, std::string f, std::string m)
			: Error{ std::move(m) }, bytePosition{ b }, fileName{ std::move(f) } {}

		virtual void print(std::ostream& o) const override;

	private:
		u64 bytePosition;
		std::string fileName;
	};

	class ValidationError : public Error {
	public:
		ValidationError(std::string f, std::string m)
			: Error{ std::move(m) }, fileName{ std::move(f) } {}

		virtual void print(std::ostream& o) const override;

	private:
		std::string fileName;
	};

	class LinkError : public Error {
	public:
		LinkError(std::string mod, std::string in, std::string m)
			: Error{ std::move(m) }, moduleName{ std::move(mod) }, importItemName{ std::move(in) } {}

		virtual void print(std::ostream& o) const override;

	private:
		std::string moduleName;
		std::string importItemName;
	};

	class CompileError : public Error {
	public:
		CompileError(std::string mod, std::string m)
			: Error{ std::move(m) }, moduleName{ std::move(mod) } {}

		CompileError(std::string mod, ModuleFunctionIndex fi, std::string m)
			: Error{ std::move(m) }, moduleName{ std::move(mod) }, functionIndex{ fi } {}

		virtual void print(std::ostream& o) const override;

	private:
		std::string moduleName;
		std::optional<ModuleFunctionIndex> functionIndex;
	};

	class LookupError : public Error {
	public:
		LookupError(std::string mod, std::string m)
			: Error{ std::move(m) }, moduleName{ mod } {}

		LookupError(std::string mod, std::string item, std::string m)
			: Error{ std::move(m) }, moduleName{ mod }, itemName{ std::move(item) } {}

		virtual void print(std::ostream& o) const override;

	private:
		std::string moduleName;
		std::optional<std::string> itemName;
	};
}

std::ostream& operator<<(std::ostream&, const WASM::Error&);