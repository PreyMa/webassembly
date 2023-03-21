#pragma once

#include <vector>
#include <optional>

#include "util.h"

namespace WASM {

	template<typename T>
	class ArrayList {
	public:
		ArrayList() = default;
		~ArrayList() = default;

		sizeType add(T value) {
			if (freeList == NilValue) {
				storage.emplace_back(NilValue, std::move(value));
				return storage.size() -1;
			}

			auto entryIdx = freeList;
			auto& entry = storage[entryIdx];
			freeList = entry.next;

			entry.next = NilValue;
			entry.data.emplace(std::move(value));
			return entryIdx;
		}

		sizeType add(sizeType nextEntry, T value) {
			auto entry = add(value);
			storage[entry].next = nextEntry;
			return entry;
		}
		
		std::optional<sizeType> remove(sizeType entryIdx) {
			auto& entry = storage[entryIdx];
			entry.data.reset();
			std::optional<sizeType> nextEntry;
			if (entry.next != NilValue) {
				nextEntry = entry.next;
			}

			entry.next = freeList;
			freeList = entryIdx;

			return nextEntry;
		}

		T& operator[](sizeType entry) {
			assert(storage[entry].data.has_value());
			return *storage[entry].data;
		}

		const T& operator[](sizeType entry) const {
			assert(storage[entry].data.has_value());
			return *storage[entry].data;
		}

		sizeType storedEntries() const {
			sizeType numFreedEntries = 0;
			sizeType ptr = freeList;
			while (ptr != NilValue) {
				numFreedEntries++;
				ptr = storage[ptr].next;
			}

			return storage.size() - numFreedEntries;
		}

		void clear() {
			storage.clear();
			freeList = NilValue;
		}

	private:
		static constexpr sizeType NilValue = (sizeType)(-1);

		struct Entry {
			sizeType next;
			std::optional<T> data;
		};

		std::vector<Entry> storage;
		sizeType freeList{ NilValue };
	};
}
