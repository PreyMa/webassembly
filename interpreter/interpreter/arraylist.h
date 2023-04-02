#pragma once

#include <vector>
#include <optional>

#include "util.h"

namespace WASM {

	/*
	* Array List class
	* Allows for the creation of multiple linked lists backed by
	* a linear array. It is intended for quick creation and consumption
	* of temporary lists. All linking references are indices into the
	* array instead of absolute addresses/pointers to keep them valid
	* when the underlying vector resizes and reallocates.
	*/
	template<typename T>
	class ArrayList {
	public:
		ArrayList() = default;
		~ArrayList() = default;

		sizeType add(T value) {
			// Free list is empty -> allocate new item
			if (freeList == NilValue) {
				storage.emplace_back(NilValue, std::move(value));
				return storage.size() -1;
			}

			// Take the first item from the free list
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
			// Destruct item contents
			auto& entry = storage[entryIdx];
			entry.data.reset();

			// Get following item if one exists
			std::optional<sizeType> nextEntry;
			if (entry.next != NilValue) {
				nextEntry = entry.next;
			}

			// Put removed item into free list
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
			// All items are either used in a linked list or are
			// in the free list -> count the free list and subtract
			// it from the total item count
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
