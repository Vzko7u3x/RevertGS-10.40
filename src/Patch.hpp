#pragma once

#include "Pch.hpp"

namespace Revert::Patch
{
	inline void Attach(void** slot, void* detour)
	{
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(slot, detour);
		DetourTransactionCommit();
	}

	inline void SwapVft(void* instance, int index, void* detour, void** saved)
	{
		if (!instance || index < 0)
			return;

		auto** table = *reinterpret_cast<void***>(instance);
		if (saved && !*saved)
			*saved = table[index];

		DWORD old = 0;
		VirtualProtect(&table[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
		table[index] = detour;
		VirtualProtect(&table[index], sizeof(void*), old, &old);
	}

	inline int ExecIndex(UFunction* function)
	{
		if (!function || !function->Func)
			return -1;

		const auto name = function->GetName();
		const bool skipValidate = name.size() >= 6 && name.compare(0, 6, "Server") == 0;

		auto* bytes = reinterpret_cast<uint8_t*>(function->Func);
		int first = -1;
		for (int i = 0; i < 0x400; ++i)
		{
			if (bytes[i] == 0xC3 && first >= 0)
				break;

			if (bytes[i] == 0x48 && bytes[i + 1] == 0xFF && bytes[i + 2] == 0xA0)
				return static_cast<int>(*reinterpret_cast<uint32_t*>(bytes + i + 3));

			if (bytes[i] == 0xFF && (bytes[i + 1] == 0x90 || bytes[i + 1] == 0x93))
			{
				const int slot = static_cast<int>(*reinterpret_cast<uint32_t*>(bytes + i + 2));
				if (first < 0)
				{
					first = slot;
					if (skipValidate)
						continue;
				}
				return slot;
			}
		}
		return first;
	}

	inline void BindExec(UObject* cdo, UFunction* function, void* detour, void** saved = nullptr, int forced = -1)
	{
		if (!cdo || !function)
			return;

		const int index = forced >= 0 ? forced : ExecIndex(function);
		if (index < 0)
		{
			Note("no exec slot for {}", function->GetName());
			return;
		}

		SwapVft(cdo, index / 8, detour, saved);
	}

	inline bool Yes() { return true; }
	inline bool No() { return false; }
	inline void Quiet() {}

	inline void PatchByte(void* at, uint8_t value)
	{
		DWORD old = 0;
		VirtualProtect(at, 1, PAGE_EXECUTE_READWRITE, &old);
		*static_cast<uint8_t*>(at) = value;
		VirtualProtect(at, 1, old, &old);
	}

	inline uint8_t* FindBytes(const uint8_t* needle, size_t nlen, size_t span = 0x4000000)
	{
		auto* start = reinterpret_cast<uint8_t*>(Rva::Base());
		auto* end = start + span;
		for (auto* p = start; p + nlen < end; ++p)
		{
			bool ok = true;
			for (size_t i = 0; i < nlen; ++i)
			{
				if (needle[i] != 0xCC && p[i] != needle[i])
				{
					ok = false;
					break;
				}
			}
			if (ok)
				return p;
		}
		return nullptr;
	}

	using ResetFn = void(*)(AFortPlayerController*);

	inline ResetFn FindReset()
	{
		static ResetFn fn = nullptr;
		static bool scanned = false;
		if (scanned)
			return fn;
		scanned = true;

		static const uint8_t needle[] = {
			0x48, 0x89, 0x5C, 0x24, 0xCC, 0x57, 0x48, 0x83, 0xEC, 0xCC,
			0x48, 0x8B, 0x91, 0xCC, 0xCC, 0xCC, 0xCC, 0x48, 0x8B, 0xF9,
			0x48, 0x85, 0xD2, 0x74, 0xCC, 0x48, 0x8B, 0x01
		};
		if (auto* hit = FindBytes(needle, sizeof(needle)))
		{
			fn = reinterpret_cast<ResetFn>(hit);
			Note("late reset +{:x}", reinterpret_cast<uintptr_t>(hit) - Rva::Base());
		}
		return fn;
	}

	inline void UnlockMatchmaking()
	{
		// 10.40 compare-then-jg that rejects extra sessions — flip jg to je.
		static const uint8_t a[] = { 0x83, 0xBD, 0xCC, 0xCC, 0xCC, 0xCC, 0x01, 0x7F, 0x18 };
		static const uint8_t b[] = { 0x83, 0x7D, 0x88, 0x01, 0x7F, 0x0D, 0x48, 0x8B, 0xCE };
		uint8_t* hit = FindBytes(a, sizeof(a));
		if (!hit)
			hit = FindBytes(b, sizeof(b));
		if (!hit)
			return;

		for (int i = 0; i < 12; ++i)
		{
			if (hit[i] == 0x7F)
			{
				PatchByte(hit + i, 0x74);
				Note("session compare patched at +{:x}", reinterpret_cast<uintptr_t>(hit + i) - Rva::Base());
				return;
			}
		}
	}
}
