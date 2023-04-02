#pragma once

#include <tuple>

#include "module.h"

namespace WASM {

	class HostFunctionBase : public Function {
	public:

		HostFunctionBase(u32, FunctionType);

		virtual const FunctionType& functionType() const final { return mFunctionType; }
		virtual Nullable<const HostFunctionBase> asHostFunction() const final { return *this; }

		void setIndex(u32 idx) { mIndex = idx; }
		void print(std::ostream&) const;

	protected:

		FunctionType mFunctionType;
	};

	template<typename TTyper>
	class HostFunction final : public HostFunctionBase {
	public:

		template<typename TLambda>
		HostFunction(TLambda lambda)
			: HostFunctionBase{ -1, toFunctionType() }, function{ std::move(lambda) } {}

	protected:
		FunctionType toFunctionType() const {
			auto parameters = ParameterTypeBuilder<typename TTyper::Parameters>::buildVector();
			auto results = ResultTypeBuilder<typename TTyper::Result>::buildVector();

			return { parameters, results };
		}

	private:
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

		std::function<typename TTyper::FunctionType> function;
	};

	template<typename TLambda>
	HostFunction(TLambda) -> HostFunction<Detail::LambdaTyper<decltype(&TLambda::operator())>>;

	//template<typename TLambda>
	//HostFunction(TLambda*) -> HostFunction<>;
}
