#include <fstream>
#include <cassert>

#include "buffer.h"

using namespace WASM;

template<typename T>
class LEB128Decoder {
public:

	static constexpr u32 ByteCount = sizeof(T);

	// Signed decoding
	template<typename TStream, typename TSFINAEHelper = T>
	std::enable_if_t<!std::is_unsigned<TSFINAEHelper>::value, T> decode(TStream& stream) const {
		T value = 0;
		u32 shift = 0;
		for (u32 i = 0; i != ByteCount; i++) {
			auto byte = stream.nextU8();
			value |= (byte & 0x7F) << shift;
			if ((byte & 0x80) == 0) {
				if (shift < 32 && (byte & 0x40) != 0) {
					return value | (~0 << shift);
				}
				return value;
			}
			shift += 7;
		}

		return value;
	}

	// Unsigned decoding
	template<typename TStream, typename TSFINAEHelper = T>
	std::enable_if_t<std::is_unsigned<TSFINAEHelper>::value, T> decode(TStream& stream) const {
		T value = 0;
		u32 shift = 0;
		for (u32 i = 0; i != 4; i++) {
			auto byte = stream.nextU8();
			value |= (byte & 0x7F) << shift;
			if ((byte & 0x80) == 0) {
				break;
			}
			shift += 7;
		}

		return value;
	}
};


Buffer Buffer::fromFile(const std::string& path)
{
	std::ifstream file{ path, std::ios::binary };
	if (!file.is_open() || !file.good()) {
		throw std::runtime_error("Coudl not open module file");
	}

	std::vector<u8> vecBuffer{ std::istreambuf_iterator<char>{ file }, {} };
	return { std::move(vecBuffer) };
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
	mData = std::move(other.mData);
	return *this;
}

BufferSlice Buffer::slice(u32 from, u32 to)
{
	assert(from <= to);
	assert(from <= size() && to <= size());
	return { mData.data()+ from, to- from };
}

BufferIterator Buffer::iterator()
{
	return { mData.data(), mData.data()+ mData.size() };
}

BufferSlice BufferSlice::slice(u32 from, u32 to)
{
	assert(from <= to);
	assert(from <= mLength && to <= mLength);
	return { mBegin + from, to - from };
}

BufferIterator BufferSlice::iterator()
{
	return { mBegin, mBegin + mLength };
}

std::string BufferSlice::toString()
{
	return std::string{ (char*)mBegin, mLength };
}

u32 BufferIterator::remaining() const
{
	return mEndPosition - mPosition;
}

bool BufferIterator::hasNext(u32 num) const
{
	return (mPosition + num) <= mEndPosition;
}

u8 BufferIterator::nextU8()
{
	assert(hasNext());
	return *(mPosition++);
}

u8 BufferIterator::peekU8() const
{
	assert(hasNext());
	return *mPosition;
}

void BufferIterator::assertU8(u8 expectedByte)
{
	if (nextU8() != expectedByte) {
		throw std::runtime_error{ "Found unexpected byte" };
	}
}

u32 BufferIterator::nextU32()
{
	return LEB128Decoder<u32>{}.decode(*this);
}

u64 BufferIterator::nextU64()
{
	return LEB128Decoder<u64>{}.decode(*this);
}

i32 BufferIterator::nextI32()
{
	return LEB128Decoder<i32>{}.decode(*this);
}

i64 BufferIterator::nextI64()
{
	return LEB128Decoder<i64>{}.decode(*this);
}

f32 BufferIterator::nextF32()
{
	auto data = nextLittleEndianU32();
	return reinterpret_cast<f32&>(data);
}

f64 BufferIterator::nextF64()
{
	auto data = nextLittleEndianU64();
	return reinterpret_cast<f64&>(data);
}

u32 BufferIterator::nextBigEndianU32()
{
	assert(hasNext(4));
	u32 value = 0;
	value |= *(mPosition + 0) << 24;
	value |= *(mPosition + 1) << 16;
	value |= *(mPosition + 2) << 8;
	value |= *(mPosition + 3) << 0;
	mPosition += 4;
	return value;
}

u32 BufferIterator::nextLittleEndianU32()
{
	assert(hasNext(4));
	u32 value = *(reinterpret_cast<u32*>(mPosition));
	mPosition += 4;
	return value;
}

u64 BufferIterator::nextLittleEndianU64()
{
	assert(hasNext(4));
	u64 value = *(reinterpret_cast<u64*>(mPosition));
	mPosition += 8;
	return value;
}

BufferSlice BufferIterator::slice()
{
	return { mPosition, remaining() };
}

BufferSlice BufferIterator::nextSliceOf(u32 length)
{
	assert(hasNext(length));
	auto slice = BufferSlice{ mPosition, length };
	mPosition += length;
	return slice;
}

BufferSlice BufferIterator::nextSliceTo(const BufferIterator& newPosition)
{
	assert(newPosition.mPosition >= mPosition && newPosition.mPosition <= mEndPosition);
	auto slice = BufferSlice{ mPosition, static_cast<u32>(newPosition.mPosition - mPosition) };
	mPosition = newPosition.mPosition;
	return slice;
}

BufferSlice BufferIterator::sliceFrom(const BufferIterator& from) const {
	assert(hasSameBase(from));
	assert(from.mPosition <= mPosition);
	return BufferSlice{ from.mPosition, static_cast<u32>(mPosition - from.mPosition) };
}

const u8* BufferIterator::positionPointer() const
{
	return mPosition;
}

void BufferIterator::moveTo(const u8* p)
{
	assert(mPosition <= p && p <= mEndPosition);
	mPosition = const_cast<u8*>(p);
}

i32 BufferIterator::operator-(const BufferIterator& other) const
{
	return mPosition - other.mPosition;
}

bool BufferIterator::operator<(const BufferIterator& other) const
{
	return mPosition < other.mPosition;
}

bool BufferIterator::operator==(const BufferIterator& other) const
{
	return mPosition == other.mPosition;
}

BufferIterator BufferIterator::operator+(u32 offset) const
{
	assert(offset <= remaining());
	return { mPosition + offset, mEndPosition };
}

BufferIterator& BufferIterator::operator+=(u32 offset)
{
	assert(offset <= remaining());
	mPosition += offset;
	return *this;
}

bool BufferIterator::hasSameBase(const BufferIterator& other) const
{
	// Check if the pointer intevals overlap. If the pointers are in valid ranges
	// of their buffer data, they therefore point into the same buffer.
	return (mPosition <= other.mEndPosition) && (other.mPosition <= mEndPosition);
}
