#pragma once

#include "forward.h"
#include "util.h"

namespace WASM {

    template<typename T>
    class VirtualSpan {
    public:
        class Iterator {
        public:
            Iterator(T* p, sizeType s) : mPosition{ p }, mStride{ s } {}

            T& operator*() const { return *mPosition; }
            bool operator!=(const Iterator& other) const { return mPosition != other.mPosition; }
            Iterator& operator++() {
                *(u8**)(&mPosition) += mStride;
                return *this;
            }

        private:
            T* mPosition;
            const sizeType mStride;
        };

        template<typename U>
        VirtualSpan(U& span)
            : mBegin{ span.data() }, mEnd{ span.data() + span.size() }, mStride{ sizeof(typename U::value_type) } {}

        sizeType size() const { return mEnd - mBegin; }
        Iterator begin() { return { mBegin, mStride }; }
        Iterator end() { return { mEnd, mStride }; }

    private:
        T* const mBegin;
        T* const mEnd;
        const sizeType mStride;
    };
}
