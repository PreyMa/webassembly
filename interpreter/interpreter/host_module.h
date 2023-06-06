#pragma once

#include <string>
#include <string_view>

#include "forward.h"
#include "host_function.h"

namespace WASM {
	class HostMemory final : public MemoryType {
	public:
		HostMemory(u32 min) : MemoryType{ Limits{min} } {}
		HostMemory(u32 min, u32 max) : MemoryType{ Limits{min, max} } {}

		template<typename T>
		std::span<T> memoryView() {
			assert(mLinkedInstance.has_value());
			return { (T*) mLinkedInstance->pointer(0), mLinkedInstance->currentSizeInBytes() / sizeof(T) };
		}

	private:
		friend class HostModule;

		void setLinkedInstance(Memory& m) { mLinkedInstance = m; }

		Nullable<Memory> mLinkedInstance;
	};

	class HostGlobal final : public DeclaredGlobalBase {
	public:
		HostGlobal(GlobalType t, u64 v)
			: DeclaredGlobalBase{ t }, mInitValue{ v } {}

		u64 initValue() const { return mInitValue; }

	private:
		friend class HostModule;

		void setLinkedInstance(GlobalBase& g) { mLinkedInstance = g; }

		u64 mInitValue;
		Nullable<GlobalBase> mLinkedInstance;
	};

	struct NamedHostMemory {
		std::string name;
		HostMemory memory;
	};

	class HostModuleBuilder {
	public:
		HostModuleBuilder(std::string n) : mName{ std::move(n) } {}

		template<typename TLambda>
		HostModuleBuilder& defineFunction(std::string name, TLambda lambda) {
			std::unique_ptr<HostFunctionBase> hostFunction = makeUniqueHostFunction(std::move(lambda));
			auto [elem, didInsert]= mFunctions.emplace(std::move(name), std::move(hostFunction));
			if (!didInsert) {
				throw std::runtime_error{ "A hostfunction with this name already exists" };
			}
			return *this;
		}

		HostModuleBuilder& defineGlobal(std::string name, ValType type, u64 initValue= 0, bool isMutable= true);
		HostModuleBuilder& defineMemory(std::string name, u32 minSize, std::optional<u32> maxSize = {});

		HostModule toModule(Interpreter&);

	private:
		std::string mName;
		std::unordered_map<std::string, std::unique_ptr<HostFunctionBase>> mFunctions;
		std::unordered_map<std::string, HostGlobal> mGlobals;
		std::optional<NamedHostMemory> mMemory;
	};

	class HostModule : public ModuleBase {
	public:
		HostModule(
			Interpreter&,
			std::string,
			SealedUnorderedMap<std::string, std::unique_ptr<HostFunctionBase>>,
			SealedUnorderedMap<std::string, HostGlobal>,
			SealedOptional<NamedHostMemory>
		);

		HostModule(HostModule&&) = default;

		virtual Nullable<HostModule> asHostModule() final { return *this; }
		virtual std::string_view name() const override { return mName; }
		virtual Nullable<Function> exportedFunctionByName(const std::string&) override;
		virtual Nullable<FunctionTable> exportedTableByName(const std::string&) override;
		virtual Nullable<Memory> exportedMemoryByName(const std::string&) override;
		virtual std::optional<ResolvedGlobal> exportedGlobalByName(const std::string&) override;

		NonNull<HostGlobal> hostGlobalByName(const std::string&);
		NonNull<HostMemory> hostMemoryByName(const std::string&);

		friend class ModuleLinker;
		friend class HostModuleHandle;

	private:
		virtual void instantiate(ModuleLinker&, Nullable<Introspector>) override;
		void createMemory(ModuleLinker&, Nullable<Introspector>);
		void createGlobals(ModuleLinker&, Nullable<Introspector>);

		virtual void initializeInstance(ModuleLinker&, Nullable<Introspector>) override;

		std::string mName;
		SealedUnorderedMap<std::string, std::unique_ptr<HostFunctionBase>> mHostFunctions;
		SealedUnorderedMap<std::string, HostGlobal> mHostGlobals;
		SealedOptional<NamedHostMemory> mHostMemory;
	};

	class HostModuleHandle {
	public:
		HostModuleHandle(HostModule& mod) : module{ mod } {}

		NonNull<HostGlobal> hostGlobalByName(const std::string& name) { return module->hostGlobalByName(name); }
		NonNull<HostMemory> hostMemoryByName(const std::string& name) { return module->hostMemoryByName(name); }

	private:
		NonNull<HostModule> module;
	};
}
