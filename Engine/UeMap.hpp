#pragma once

// Minimal map used by the dumped DataTable::RowMap field.

namespace UE
{
	template <typename Key, typename Value>
	class TMap
	{
	public:
		struct Pair
		{
			Key   key;
			Value value;
			int32_t next = 0;
		};

		Pair*   pairs = nullptr;
		int32_t count = 0;
		int32_t slack = 0;
		char    pad[0x40]{};

		Value operator[](const Key& key) const
		{
			for (int32_t i = 0; i < count; ++i)
			{
				if (pairs[i].key == key)
					return pairs[i].value;
			}
			return Value{};
		}
	};
}
