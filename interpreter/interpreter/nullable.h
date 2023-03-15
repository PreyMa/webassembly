#pragma once

namespace WASM {
	template<typename T>
	class Nullable {
	public:
		Nullable() = default;
		Nullable(T& x) : ptr{ &x } {};

		bool has_value() const { return ptr != nullptr; }
		T& value() { return *ptr; }
		const T& value() const { return *ptr; }

		T& operator*() { return value(); }
		const T& operator*() const { return value(); }

		T* operator->() { return ptr; }
		const T* operator->() const { return ptr; }

	private:
		T* ptr{ nullptr };
	};
}
