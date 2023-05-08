#pragma once

#include <span>

#include "util.h"

namespace WASM {
    template<typename T, int Idx>
    struct TypedIndex {
        TypedIndex() = delete;
        constexpr TypedIndex(const TypedIndex& x) : value{ x.value } {}
        explicit constexpr TypedIndex(T x) : value{ x } {}

        TypedIndex& operator=(const TypedIndex& x) { value = x.value; return *this; }

        constexpr int operator<=>(const TypedIndex& x) const { return value - x.value; }
        constexpr int operator<=>(T x) const { return value - x; }

        constexpr bool operator==(const TypedIndex& x) const { return value == x.value; }
        constexpr bool operator==(T x) const { return value == x; }

        TypedIndex operator+(T off) { return TypedIndex{ value + off }; }
        TypedIndex operator-(T off) { return TypedIndex{ value - off }; }

        TypedIndex operator+=(T off) { value += off; return *this; }
        TypedIndex operator-=(T off) { value -= off; return *this; }

        TypedIndex operator++(int) { return TypedIndex{ value++ }; }
        TypedIndex operator--(int) { return TypedIndex{ value-- }; }

        using TStorage = T;
        T value;
    };

    template<typename TIndex, typename TElement>
    struct IndexSpan {
        TIndex mBegin{ 0 };
        TIndex mEnd{ 0 };

        void init(std::span<TElement> current, sizeType newItems) {
            mBegin = TIndex{ (typename TIndex::TStorage) current.size() };
            mEnd = mBegin + newItems;
        }

        sizeType size() const { return mEnd.value - mBegin.value; }
        std::span<TElement> span(std::span<TElement> sp) const {
            return sp.subspan(mBegin.value, size());
        }
        std::span<const TElement> constSpan(std::span<const TElement> sp) const {
            return sp.subspan(mBegin.value, size());
        }
    };

    using ModuleTypeIndex= TypedIndex<u32, 0>;
    using ModuleFunctionIndex = TypedIndex<u32, 1>;
    using ModuleMemoryIndex = TypedIndex<u32, 2>;
    using ModuleTableIndex = TypedIndex<u32, 3>;
    using ModuleGlobalIndex = TypedIndex<u32, 4>;
    using ModuleElementIndex = TypedIndex<u32, 5>;
    using ModuleDataIndex = TypedIndex<u32, 6>;

    using InterpreterTypeIndex = TypedIndex<u32, 10>;
    using InterpreterFunctionIndex = TypedIndex<u32, 11>;
    using InterpreterMemoryIndex = TypedIndex<u32, 12>;
    using InterpreterTableIndex = TypedIndex<u32, 13>;
    using InterpreterLinkedElementIndex = TypedIndex<u32, 15>;

    using LocalFunctionIndex = TypedIndex<u32, 20>;             // References a local function in a module disregarding any imported functions

    using InterpreterGlobalTypedArrayIndex = TypedIndex<u32, 21>;   // References a global either in a vector<u32> or vector<u64> depending on the global's type
    using ModuleExportIndex = TypedIndex<u32, 22>;                  // References either an exported function, table, memory or global by its module index, based on the export's type
}

// Allow for stream printing eg. std::cout
template<typename TStream, typename T, int Idx>
TStream& operator<<(TStream& stream, const WASM::TypedIndex<T, Idx>& typedIdx) {
    stream << typedIdx.value;
    return stream;
}
