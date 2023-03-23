#pragma once

#include <tuple>

#include "module.h"

namespace WASM {

	class HostFunctionBase : public Function {
	public:

		HostFunctionBase(FunctionType);

		virtual const FunctionType& functionType() const final { return mFunctionType; }
		virtual Nullable<const HostFunctionBase> asHostFunction() const final { return *this; }

		void print(std::ostream&) const;

	protected:

		FunctionType mFunctionType;
	};

	template<typename TTyper>
	class HostFunction final : public HostFunctionBase {
	public:

		template<typename TLambda>
		HostFunction(TLambda lambda)
			: HostFunctionBase{ toFunctionType() }, function{ std::move(lambda) } {}

	protected:
		FunctionType toFunctionType() const {
			auto parameters = ParameterTypeBuilder<typename TTyper::Parameters>::buildVector();
			auto results = ResultTypeBuilder<typename TTyper::Result>::buildVector();

			return { std::move(parameters), std::move(results) };
		}

	private:
		template<typename ...Us>
		static std::vector<ValType> buildValTypeVector() {
			std::vector<ValType> vec;
			vec.reserve(sizeof...(Us));

			if constexpr (sizeof...(Us) > 0) {
				buildValTypeVectorIterate<Us...>(vec);
			}

			return vec;
		}

		template<typename U, typename ...Us>
		static void buildValTypeVectorIterate(std::vector<ValType>& vec) {
			vec.emplace_back( ValType::fromType<U>() );

			if constexpr (sizeof...(Us) > 0) {
				buildValTypeVectorIterate<Us...>(vec);
			}
		}

		template<typename U>
		struct ResultTypeBuilder {
			static std::vector<ValType> buildVector() {
				std::vector<ValType> vec;
				vec.reserve(1);
				buildValTypeVectorIterate<U>(vec);
				return vec;
			}
		};

		template<>
		struct ResultTypeBuilder<void> {
			static std::vector<ValType> buildVector() {
				return {};
			}
		};

		template<typename ...Us>
		struct ResultTypeBuilder<std::tuple<Us...>> {
			static std::vector<ValType> buildVector() {
				return buildValTypeVector<Us...>();
			}
		};

		template<typename>
		struct ParameterTypeBuilder {};

		template<typename ...Us>
		struct ParameterTypeBuilder<Detail::ParameterPack<Us...>> {
			static std::vector<ValType> buildVector() {
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
