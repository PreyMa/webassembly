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

		operator std::span<T>() { return { vector }; }
		operator std::span<const T>() const { return { vector }; }

		std::optional<sizeType> indexOfPointer(const T* ptr) const {
			if (!vector.empty() && (ptr >= vector.data()) && (ptr < vector.data() + vector.size())) {
				return { ptr - vector.data() };
			}

			return {};
		}

	private:
		std::vector<T> vector;
	};

	template<typename K, typename V>
	class SealedUnorderedMap {
	public:
		SealedUnorderedMap(std::unordered_map<K, V> m) : map{ std::move(m) } {}
		SealedUnorderedMap() = default;
		SealedUnorderedMap(SealedUnorderedMap&&) = default;
		SealedUnorderedMap(const SealedUnorderedMap&) = delete;

		auto find(const K& key) { return map.find(key); }
		auto find(const K& key) const { return map.find(key); }

		sizeType size() const { return map.size(); }
		auto begin() { return map.begin(); }
		auto end() { return map.end(); }
		auto begin() const { return map.begin(); }
		auto end() const { return map.end(); }

		SealedUnorderedMap& operator=(SealedUnorderedMap m) { map = std::move(m.map); return *this; }

	private:
		std::unordered_map<K, V> map;
	};
}
