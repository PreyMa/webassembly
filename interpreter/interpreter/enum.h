#pragma once

#include "util.h"

namespace WASM {
	template<typename TSpecial, typename TStorage= u32>
	class Enum {
	public:
		using TEnumStorage = TStorage;

		template<typename T>
		static TSpecial fromInt(T x) {
			static_assert(TSpecial::TEnum::NumberOfItems < ((TStorage)~0));
			assert(x < TSpecial::TEnum::NumberOfItems);
			return TSpecial{ (TStorage)x };
		}

		explicit Enum(TStorage v) : value{ v } {}
		operator int() const { return value; }

		TSpecial operator=(TSpecial other) { value = other.value; return *this; }

	protected:
		TStorage value;
	};

	class SectionType : public Enum<SectionType> {
	public:
		enum TEnum {
			Custom = 0,
			Type = 1,
			Import = 2,
			Function = 3,
			Table = 4,
			Memory = 5,
			GlobalType = 6,
			Export = 7,
			Start = 8,
			Element = 9,
			Code = 10,
			Data = 11,
			DataCount = 12,
			NumberOfItems = 13
		};

		using Enum<SectionType>::Enum;
		SectionType(TEnum e) : Enum<SectionType>{ e } {}

		const char* name() const;
	};

	class ValType : public Enum<ValType, u8> {
	public:
		enum TEnum {
			I32 = 0x7F,
			I64 = 0x7E,
			F32 = 0x7D,
			F64 = 0x7C,
			V128 = 0x7B,
			FuncRef = 0x70,
			ExternRef = 0x6F,
			NumberOfItems = 0x80
		};

		using Enum<ValType, u8>::Enum;
		ValType(TEnum e) : Enum<ValType, u8>{ e } {}

		// Requried for the local array in FunctionType
		ValType() : Enum<ValType, u8>{ TEnum::I32 } {}

		bool isNumber() const;
		bool isVector() const;
		bool isReference() const;
		bool isValid() const;
		const char* name() const;
		u32 sizeInBytes() const;

		template<typename T>
		static ValType fromType() {
			static_assert(sizeof(T) == 0, "Unsupported val type");
		}

		template<> static ValType fromType<u32>() { return ValType::I32; }
		template<> static ValType fromType<i32>() { return ValType::I32; }
		template<> static ValType fromType<u64>() { return ValType::I64; }
		template<> static ValType fromType<i64>() { return ValType::I64; }
		template<> static ValType fromType<f32>() { return ValType::F32; }
		template<> static ValType fromType<f64>() { return ValType::F64; }
	};

	class ExportType : public Enum<ExportType> {
	public:
		enum TEnum {
			FunctionIndex = 0x00,
			TableIndex = 0x01,
			MemoryIndex = 0x02,
			GlobalIndex = 0x03,
			NumberOfItems
		};

		using Enum<ExportType>::Enum;
		ExportType(TEnum e) : Enum<ExportType>{ e } {}

		const char* name() const;
	};

	class ElementMode : public Enum<ElementMode> {
	public:
		enum TEnum {
			Passive = 0,
			Active = 1,
			Declarative = 2,
			NumberOfItems
		};

		using Enum<ElementMode>::Enum;
		ElementMode(TEnum e) : Enum<ElementMode>{ e } {}

		const char* name() const;
	};

	class NameSubsectionType : public Enum<NameSubsectionType> {
	public:
		enum TEnum {
			// Based on the extended name section proposal: https://github.com/WebAssembly/extended-name-section/blob/main/proposals/extended-name-section/Overview.md
			ModuleName = 0,
			FunctionNames = 1,
			LocalNames = 2,
			LabelNames = 3,
			TypeNames = 4,
			TableNames = 5,
			MemoryNames = 6,
			GlobalNames = 7,
			ElementSegmentNames = 8,
			DataSegmentNames = 9,
			NumberOfItems
		};

		using Enum<NameSubsectionType>::Enum;
		NameSubsectionType(TEnum e) : Enum<NameSubsectionType>{ e } {}

		const char* name() const;
	};

	class BlockType : public Enum<BlockType> {
	public:
		enum TEnum {
			None,
			ValType,
			TypeIndex,
			NumberOfItems
		};

		using Enum<BlockType>::Enum;
		BlockType(TEnum e) : Enum<BlockType>{ e } {}

		const char* name() const;
	};

	class ImportType : public Enum<ImportType> {
	public:
		enum TEnum {
			FunctionImport = 0x00,
			TableImport = 0x01,
			MemoryImport = 0x02,
			GlobalImport = 0x03,
			NumberOfItems
		};

		using Enum<ImportType>::Enum;
		ImportType(TEnum e) : Enum<ImportType>{ e } {}

		const char* name() const;
	};
}
