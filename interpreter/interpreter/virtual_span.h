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

        using value_type = T;
        using iterator = Iterator;

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

    template<typename T>
    class VirtualForwardIterator {
    public:
        class Proxy {
        public:
            Proxy(VirtualForwardIterator* ptr = nullptr) : base{ ptr } {}
            void operator++() { base->next(); }
            T& operator*() { return base->get(); }
            bool operator!=(const Proxy& x) {
                if (base == x.base) {
                    return false;
                }

                auto* b = base != nullptr ? base : x.base;
                return b->hasNext();
            }
        private:
            VirtualForwardIterator* base;
        };

        virtual T& get() = 0;
        T& operator*() { return get(); }

        virtual bool hasNext() const = 0;
        virtual void next() = 0;

        Proxy begin() { return { this }; }
        Proxy end() { return {}; }
    };

    template<typename TContainer>
    class TypedVirtualForwardIterator final : public VirtualForwardIterator<typename TContainer::value_type> {
    public:
        TypedVirtualForwardIterator(TContainer& container) : mBegin{ container.begin() }, mEnd{ container.end() } {}

        virtual typename TContainer::value_type& get() override { return *mBegin; }
        virtual bool hasNext() const { return mBegin != mEnd; }
        virtual void next() { ++mBegin; }

    private:
        typename TContainer::iterator mBegin, mEnd;
    };

    template<typename TKey, typename TItem>
    class TypedVirtualForwardIterator<std::unordered_map<TKey, TItem>> final : public VirtualForwardIterator<TItem> {
    public:
        TypedVirtualForwardIterator(std::unordered_map<TKey, TItem>& container) : mBegin{ container.begin() }, mEnd{ container.end() } {}

        virtual TItem& get() override { return mBegin->second; }
        virtual bool hasNext() const { return mBegin != mEnd; }
        virtual void next() { ++mBegin; }

    private:
        typename std::unordered_map<TKey, TItem>::iterator mBegin, mEnd;
    };

    template<typename TItem>
    class TypedVirtualForwardIterator<std::optional<TItem>> final : public VirtualForwardIterator<TItem> {
    public:
        TypedVirtualForwardIterator(std::optional<TItem>& container) : mValue{ container.has_value() ? &container.value() : nullptr } {}

        virtual TItem& get() override { return *mValue; }
        virtual bool hasNext() const { return mValue; }
        virtual void next() { mValue = nullptr; }

    private:
        TItem* mValue;
    };

    template<typename TValue, typename TKey, typename TItem>
    class TypedVirtualForwardIteratorOf final : public VirtualForwardIterator<TValue> {
    public:
        TypedVirtualForwardIteratorOf(SealedUnorderedMap<TKey, TItem>& container) : mBegin{ container.begin() }, mEnd{ container.end() } {}

        virtual TValue& get() override { return mBegin->second; }
        virtual bool hasNext() const { return mBegin != mEnd; }
        virtual void next() { ++mBegin; }

    private:
        typename SealedUnorderedMap<TKey, TItem>::iterator mBegin, mEnd;
    };

    template <typename T>
    TypedVirtualForwardIterator(T&) -> TypedVirtualForwardIterator<T>;
}
