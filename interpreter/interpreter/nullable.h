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
		Nullable(const Nullable<U>& x) : ptr{ x.pointer() } {};

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

		template<typename U>
		bool operator==(const Nullable<U>& x) const { return ptr == x.ptr; }

		template<typename U>
		bool operator==(const U* x) const { return ptr == x; }

		void clear() { ptr = nullptr; }

	private:
		Nullable(T* p) : ptr{ p } {}

		T* ptr{ nullptr };
	};

	template<typename T>
	class NonNull {
	public:
		NonNull() = delete;
		NonNull(const NonNull&) = default;

		NonNull(T& x) : ptr{ &x } {};

		template<typename U>
		NonNull(const NonNull<U>& x) : ptr{ x.pointer() } {};

		T& value() { return *ptr; }
		const T& value() const { return *ptr; }

		T& operator*() { return value(); }
		const T& operator*() const { return value(); }

		T* operator->() { return ptr; }
		const T* operator->() const { return ptr; }

		T* pointer() { return ptr; }
		const T* pointer() const { return ptr; }

		NonNull operator=(T& x) { ptr = &x; return *this; };

		template<typename U>
		NonNull operator=(const NonNull<U>& x) { ptr = x.ptr; return *this; };

		template<typename U>
		bool operator==(const NonNull<U>& x) const { return ptr == x.ptr; }

		template<typename U>
		bool operator==(const U* x) const { return ptr == x; }

	private:
		T* ptr;
	};
}
