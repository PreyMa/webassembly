
#pragma once

#include <vector>
#include <string>
#include <ostream>
#include <cassert>

#include "util.h"
#include "forward.h"

namespace WASM {
	class BufferIterator {
	public:
		BufferIterator() = default;
		BufferIterator(const BufferIterator&) = default;
		BufferIterator(u8* b, u8* e) : mPosition{ b }, mEndPosition{ e } { assert(b <= e); assert((e-b) < 0x7FFFFFFF); }


		u32 remaining() const;
		bool hasNext(u32 num = 1) const;
		u8 nextU8();
		u8 peekU8() const;
		void assertU8(u8);

		u32 nextU32(); // unsigned LEB128 encoding
		u64 nextU64();
		i32 nextI32(); // signed LEB128 encoding
		i64 nextI64();

		f32 nextF32();
		f64 nextF64();

		u32 nextBigEndianU32();

		u32 nextLittleEndianU32();
		u64 nextLittleEndianU64();

		BufferSlice slice();
		BufferSlice nextSliceOf(u32);
		BufferSlice nextSliceTo(const BufferIterator&);
		BufferSlice sliceFrom(const BufferIterator&) const;

		const u8* positionPointer() const;

		void moveTo(const u8*);

		i32 operator-(const BufferIterator&) const;
		bool operator<(const BufferIterator&) const;
		bool operator==(const BufferIterator&) const;
		BufferIterator operator+(u32) const;
		BufferIterator& operator+=(u32);

		bool hasSameBase(const BufferIterator&) const;

	private:
		u8* mPosition{ nullptr };
		u8* mEndPosition{ nullptr };
	};

	class Buffer {
	public:
		static Buffer fromFile(const std::string&);

		Buffer() = default;
		Buffer( std::vector<u8> d ) : mData{ std::move(d) } {}
		Buffer( Buffer&& b ) : mData{ std::move(b.mData) } {}

		Buffer& operator=(const Buffer&) = delete;
		Buffer& operator=(Buffer&&) noexcept;

		sizeType size() const { return mData.size(); }
		bool isEmpty() const { return mData.size() == 0; }
		bool hasInRange(const u8*) const;

		void clear() { mData.clear(); }

		void appendU8(u8);
		void appendLittleEndianU32(u32);
		void appendLittleEndianU64(u64);

		void writeLittleEndianU32(sizeType, u32);

		u8& operator[](sizeType idx) { return mData[idx]; }
		const u8& operator[](sizeType idx) const { return mData[idx]; }

		BufferSlice slice(sizeType from, sizeType to);
		BufferIterator iterator();

		const u8* begin() const { return mData.data(); }
		const u8* end() const { return mData.data()+ mData.size(); }

	private:
		std::vector<u8> mData;
	};


	class BufferSlice {
	public:
		BufferSlice(u8* b, sizeType l) : mBegin{ b }, mLength{ l } {}

		sizeType size() const { return mLength; }
		bool isEmpty() const { return mLength == 0; }

		u8& operator[](sizeType idx) { assert(idx < mLength); return mBegin[idx]; }
		const u8& operator[](sizeType idx) const { assert(idx < mLength); return mBegin[idx]; }

		BufferSlice slice(sizeType from, sizeType to);
		BufferIterator iterator();

		const u8* begin() const { return mBegin; }
		const u8* end() const { return mBegin + mLength; }

		const u8 first() const { return *mBegin; }
		const u8 last() const { return *(mBegin +mLength -1); }


		std::string toString();
		void print(std::ostream&) const;

	private:
		u8* mBegin;
		sizeType mLength;
	};
}

