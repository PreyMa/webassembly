#pragma once

#include <array>

#include "host_function.h"

namespace WASM {
	class Value {
	public:

		Value(ValType t, u64 data) : type{ t }, u64Data{ data } {}

		template<typename T>
		static Value fromType(T val) {
			return { ValType::fromType<T>(), reinterpret_cast<u64&>(val) };
		}

		u32 sizeInBytes() const { return type.sizeInBytes(); }

		u32 asU32() const { return u32Data; }
		u32 asU64() const { return u64Data; }

	private:
		ValType type;
		union {
			u32 u32Data;
			u64 u64Data;
			f32 f32Data;
			f64 f64Data;
			Function* refData;
		};
	};

	class ValuePack {
	public:
		ValuePack(const FunctionType& ft, bool r, std::span<u32> s)
			: functionType{ ft }, isResult{ r }, stackSlice{ s } {}

		void print(std::ostream&) const;

	private:
		const FunctionType& functionType;
		bool isResult;
		std::span<u32> stackSlice;
	};

	class FunctionHandle {
	private:
		friend class Interpreter;

		Function& function;
	};

	class Interpreter {
	public:
		Interpreter();
		~Interpreter();

		void loadModule(std::string);
		void compileAndLinkModules();

		FunctionHandle functionByName(std::string_view, std::string_view);
		
		void runStartFunctions();

		template<typename ...Args>
		ValuePack runFunction(const FunctionHandle& handle, Args... args) {
			std::array<Value, sizeof...(Args)> argumentArray{ Value::fromType<Args>( args )... };
			return executeFunction(handle.function, argumentArray);
		}

		void attachIntrospector(std::unique_ptr<Introspector>);

	private:
		friend class ModuleCompiler;

		ValuePack executeFunction(Function&, std::span<Value>);
		ValuePack runInterpreterLoop(const BytecodeFunction&, std::span<Value>);

		Nullable<Function> findFunction(const std::string&, const std::string&);

		std::vector<Module> modules;
		std::unordered_map<std::string, Nullable<Module>> moduleNameMap;

		bool hasLinked{ false };
		bool isInterpreting{ false };
		std::unique_ptr<u32[]> stackBase;

		std::unique_ptr<Introspector> attachedIntrospector;
	};
}