#pragma once

#include <cstddef>
#include <cstdint>

namespace WASM {
	using u8 = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;

	using i8 = std::int8_t;
	using i16 = std::int16_t;
	using i32 = std::int32_t;
	using i64 = std::int64_t;

	using f32 = float;
	using f64 = double;

	using sizeType = std::size_t;


	namespace Detail {

		template<typename ...T>
		struct ParameterPack {};

		// Based on https://stackoverflow.com/questions/64782121/deduce-lambda-return-and-arguments-passed-to-constructor
		template<typename>
		struct LambdaTyper;

		template<typename R, typename C, typename... Args>
		struct LambdaTyper<R(C::*)(Args...) const> {
			using FunctionType = R(Args...);
			using Result = R;
			using Parameters = ParameterPack<Args...>;
			using Class = C;
		};

		// For mutable lambdas
		template<typename R, typename C, typename... Args>
		struct LambdaTyper<R(C::*)(Args...)> {
			using FunctionType = R(Args...);
			using Result = R;
			using Parameters = ParameterPack<Args...>;
			using Class = C;
		};

		template<typename TLambda>
		using MakeLambdaTyper = LambdaTyper<decltype(&TLambda::operator())>;
	}
}
