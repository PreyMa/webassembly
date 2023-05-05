#pragma once

#include <tuple>

#include "module.h"

namespace WASM {

	class HostFunctionBase : public Function {
	public:

		HostFunctionBase(ModuleFunctionIndex, FunctionType);

		virtual const FunctionType& functionType() const final { return mFunctionType; }
		virtual Nullable<const HostFunctionBase> asHostFunction() const final { return *this; }

		void setIndex(ModuleFunctionIndex idx) { mIndex = idx; }
		void setLinkedFunctionType(InterpreterTypeIndex idx) { mInterpreterTypeIndex = idx; }
		void print(std::ostream&) const;

		virtual u32* executeFunction(u32*)= 0;

	protected:

		FunctionType mFunctionType;
	};

	template<typename TTyper>
	class HostFunction final : public HostFunctionBase {
	public:

		template<typename TLambda>
		HostFunction(TLambda lambda)
			: HostFunctionBase{ ModuleFunctionIndex{(u32)-1}, toFunctionType()}, function{std::move(lambda)} {}

		HostFunction(HostFunction&&) = default;

		virtual u32* executeFunction(u32* stackPointer) override {
			return ParameterPopper<typename TTyper::Parameters>::popParametersAndCall(stackPointer, *this);
		}

	protected:
		FunctionType toFunctionType() const {
			auto parameters = ParameterTypeBuilder<typename TTyper::Parameters>::buildVector();
			auto results = ResultTypeBuilder<typename TTyper::Result>::buildVector();

			return { parameters, results };
		}

	private:

		// Infrastructure for creating the function type

		template<typename ...Us>
		static auto buildValTypeVector() {
			std::array<ValType, sizeof...(Us)> array;

			if constexpr (sizeof...(Us) > 0) {
				buildValTypeVectorIterate<Us...>(array.data());
			}

			return array;
		}

		template<typename U, typename ...Us>
		static void buildValTypeVectorIterate(ValType* it) {
			*it= ValType::fromType<U>();

			if constexpr (sizeof...(Us) > 0) {
				buildValTypeVectorIterate<Us...>(it+ 1);
			}
		}

		template<typename U>
		struct ResultTypeBuilder {
			static auto buildVector() {
				std::array<ValType, 1> array;
				buildValTypeVectorIterate<U>(array.data());
				return array;
			}
		};

		template<>
		struct ResultTypeBuilder<void> {
			static std::array<ValType, 0> buildVector() {
				return {};
			}
		};

		template<typename ...Us>
		struct ResultTypeBuilder<std::tuple<Us...>> {
			static auto buildVector() {
				return buildValTypeVector<Us...>();
			}
		};

		template<typename>
		struct ParameterTypeBuilder {};

		template<typename ...Us>
		struct ParameterTypeBuilder<Detail::ParameterPack<Us...>> {
			static auto buildVector() {
				return buildValTypeVector<Us...>();
			}
		};

		// Stack handling infrastructure for popping/pushing call parameters/results

		template<typename ... Us, typename ...Vs>
		static u32* callAfterPoppingParameters(u32* stackPointer, HostFunction& self, Vs ... params) {
			if constexpr (sizeof...(Us) == 0) {
				return CallerAndResultPusher<typename TTyper::Result>::callAndPushResults(stackPointer, self, params...);
			}
			else {
				return popNextParameter<Us...>(stackPointer, self, params...);
			}
		}

		template<typename U, typename ... Us, typename ...Vs>
		static u32* popNextParameter(u32* stackPointer, HostFunction& self, Vs ... params) {
			static_assert(sizeof(U) % 4 == 0, "Host function parameter size not divisible by 4");
			stackPointer -= (sizeof(U) / 4);
			U param= (U) *reinterpret_cast<U*>(stackPointer);
			return callAfterPoppingParameters<Us...>(stackPointer, self, params..., param);
		}

		template<typename U>
		static u32* pushResult(u32* stackPointer, U val) {
			*reinterpret_cast<U*>(stackPointer) = val;
			static_assert(sizeof(U) % 4 == 0, "Host function return value size not divisible by 4");
			stackPointer += (sizeof(U) / 4);
			return stackPointer;
		}

		template<int Idx, typename U>
		static u32* pushResultTuple(u32* stackPointer, U& resultTuple) {
			if constexpr (Idx >= std::tuple_size_v<U>) {
				return stackPointer;
			}
			else {
				stackPointer = pushResult(stackPointer, std::get<Idx>(resultTuple));
				return pushResultTuple<Idx + 1>(stackPointer, resultTuple);
			}
		}

		template<typename>
		struct ParameterPopper {};

		template<typename ...Us>
		struct ParameterPopper<Detail::ParameterPack<Us...>> {
			static u32* popParametersAndCall(u32* stackPointer, HostFunction& self) {
				return callAfterPoppingParameters<Us...>(stackPointer, self);
			}
		};

		template<typename U>
		struct CallerAndResultPusher {
			template<typename ...Vs>
			static u32* callAndPushResults(u32* stackPointer, HostFunction& self, Vs... params) {
				auto result = self.function(params...);
				return pushResult(stackPointer, result);
			}
		};

		template<>
		struct CallerAndResultPusher<void> {
			template<typename ...Vs>
			static u32* callAndPushResults(u32* stackPointer, HostFunction& self, Vs... params) {
				self.function(params...);
				return stackPointer;
			}
		};

		template<typename ...Us>
		struct CallerAndResultPusher<std::tuple<Us...>> {
			template<typename ...Vs>
			static u32* callAndPushResults(u32* stackPointer, HostFunction& self, Vs... params) {
				auto result = self.function(params...);
				return pushResultTuple<sizeof...(Us)>(stackPointer, result);
			}
		};

		std::function<typename TTyper::FunctionType> function;
	};

	template<typename TLambda>
	HostFunction(TLambda) -> HostFunction<Detail::MakeLambdaTyper<TLambda>>;

	template<typename TLambda>
	auto makeUniqueHostFunction(TLambda lambda) {
		return std::make_unique<HostFunction<Detail::MakeLambdaTyper<TLambda>>>(std::move(lambda));
	}

	//template<typename TLambda>
	//HostFunction(TLambda*) -> HostFunction<>;
}
