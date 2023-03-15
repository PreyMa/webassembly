#pragma once

namespace WASM {
	template<typename TSpecial, typename TStorage= u32>
	class Enum {
	public:
		using TEnumStorage = TStorage;

		template<typename T>
		static TSpecial fromInt(T x) {
			assert(x < TSpecial::TEnum::NumberOfItems);
			return TSpecial{ x };
		}

		explicit Enum(TStorage v) : value{ v } {}
		operator int() const { return value; }

		TSpecial operator=(TSpecial other) { value = other.value; return *this; }

	protected:
		TStorage value;
	};
}
