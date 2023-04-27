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
			numStoredEntries++;

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
		
		std::optional<sizeType> remove(sizeType entryIdx, std::optional<sizeType> previousEntryIdx = {}) {
			numStoredEntries--;

			// Destruct item contents
			auto& entry = storage[entryIdx];
			entry.data.reset();

			// Relink the previous item if one was provided
			if (previousEntryIdx.has_value()) {
				auto& prevEntry = storage[*previousEntryIdx];
				assert(prevEntry.next == entryIdx);

				prevEntry.next = entry.next;
			}

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
			return numStoredEntries;
		}

		bool isEmpty() const {
			return !numStoredEntries;
		}

		void clear() {
			storage.clear();
			freeList = NilValue;
			numStoredEntries = 0;
		}

		std::optional<sizeType> nextOf(sizeType entry) {
			if (storage[entry].next == NilValue) {
				return {};
			}

			return storage[entry].next;
		}

		void reserve(sizeType slots) {
			storage.reserve(slots);
		}

	private:
		static constexpr sizeType NilValue = (sizeType)(-1);

		struct Entry {
			sizeType next;
			std::optional<T> data;
		};

		std::vector<Entry> storage;
		sizeType freeList{ NilValue };
		sizeType numStoredEntries{ 0 };
	};
}
