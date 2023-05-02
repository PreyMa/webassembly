#pragma once

#include <string>
#include <string_view>

#include "forward.h"
#include "host_function.h"

namespace WASM {
	class HostModuleBuilder {
	public:
		HostModuleBuilder(std::string n) : mName{ std::move(n) } {}

		template<typename TLambda>
		HostModuleBuilder& defineFunction(std::string name, TLambda lambda) {
			std::unique_ptr<HostFunctionBase> hostFunction = makeUniqueHostFunction(std::move(lambda));
			mFunctions.emplace(std::move(name), std::move(hostFunction));
			return *this;
		}

		HostModule toModule();

	private:
		std::string mName;
		std::unordered_map<std::string, std::unique_ptr<HostFunctionBase>> mFunctions;
		std::unordered_map<std::string, DeclaredHostGlobal> mGlobals;
		std::vector<Global<u32>> mGlobals32;
		std::vector<Global<u64>> mGlobals64;
	};

	class HostModule : public ModuleBase {
	public:
		HostModule(
			std::string,
			SealedUnorderedMap<std::string, std::unique_ptr<HostFunctionBase>>,
			SealedUnorderedMap<std::string, DeclaredHostGlobal>,
			SealedVector<Global<u32>>,
			SealedVector<Global<u64>>
		);

		HostModule(HostModule&&) = default;

		virtual Nullable<HostModule> asHostModule() final { return *this; }
		virtual std::string_view name() const override { return mName; }
		virtual Nullable<Function> exportedFunctionByName(const std::string&) override;
		virtual Nullable<FunctionTable> exportedTableByName(const std::string&) override;
		virtual Nullable<Memory> exportedMemoryByName(const std::string&) override;
		virtual std::optional<ResolvedGlobal> exportedGlobalByName(const std::string&) override;

		friend class ModuleLinker;

	private:
		std::string mName;
		SealedUnorderedMap<std::string, std::unique_ptr<HostFunctionBase>> mFunctions;
		SealedUnorderedMap<std::string, DeclaredHostGlobal> mGlobals;
		SealedVector<Global<u32>> mGlobals32;
		SealedVector<Global<u64>> mGlobals64;
	};
}
