#pragma once

#include <vector>

#include "util.h"

namespace WASM {
	template<typename T>
	class SealedVector {
	public:
		SealedVector(std::vector<T> vec) : vector{ std::move(vec) } {}
		SealedVector() = default;
		SealedVector(SealedVector&&) = default;
		SealedVector(const SealedVector&) = delete;

		T& operator[](sizeType idx) { return vector[idx]; }
		const T& operator[](sizeType idx) const { return vector[idx]; }

		sizeType size() const { return vector.size(); }
		auto begin() { return vector.begin(); }
		auto end() { return vector.end(); }
		auto begin() const { return vector.begin(); }
		auto end() const { return vector.end(); }

		SealedVector& operator=(SealedVector v) { vector = std::move(v.vector); return *this; }

	private:
		std::vector<T> vector;
	};
}
