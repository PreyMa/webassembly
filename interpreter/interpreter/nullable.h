#pragma once

#include <memory>

namespace WASM {
	template<typename T>
	class Nullable {
	public:
		Nullable() = default;
		Nullable(const Nullable&) = default;

		Nullable(T& x) : ptr{ &x } {};

		template<typename U>
		Nullable(const Nullable<U>& x) : ptr{ x.ptr } {};

		template<typename U>
		static Nullable<T> fromPointer(const std::unique_ptr<U>& ptr) {
			return { ptr.get() };
		}

		bool has_value() const { return ptr != nullptr; }
		T& value() { return *ptr; }
		const T& value() const { return *ptr; }

		T& operator*() { return value(); }
		const T& operator*() const { return value(); }

		T* operator->() { return ptr; }
		const T* operator->() const { return ptr; }

		T* pointer() { return ptr; }
		const T* pointer() const { return ptr; }

		Nullable operator=(T& x) { ptr = &x; return *this; };

		template<typename U>
		Nullable operator=(const Nullable<U>& x) { ptr = x.ptr; return *this; };

	private:
		Nullable(T* p) : ptr{ p } {}

		T* ptr{ nullptr };
	};
}
