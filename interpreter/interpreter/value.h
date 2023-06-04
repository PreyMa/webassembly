#pragma once

#include "util.h"

namespace WASM {
	class Value {
	public:

		Value(ValType t, u64 data) : mType{ t }, u64Data{ data } {}

		static Value fromStackPointer(ValType, std::span<u32>, u32&);

		template<typename T>
		static Value fromType(T val) {
			return { ValType::fromType<T>(), reinterpret_cast<u64&>(val) };
		}

		auto type() const { return mType; }
		u32 sizeInBytes() const { return mType.sizeInBytes(); }

		template<typename T>
		T as() {
			static_assert("Unsupported casting type for value");
		}

		template<> u32 as<u32>() { return u32Data; }
		template<> i32 as<i32>() { return u32Data; }
		template<> u64 as<u64>() { return u64Data; }
		template<> i64 as<i64>() { return u64Data; }
		template<> f32 as<f32>() { return f32Data; }
		template<> f64 as<f64>() { return f64Data; }

		u64 asInt() const;
		f64 asFloat() const;

		void print(std::ostream&) const;

	private:
		ValType mType;
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
}
