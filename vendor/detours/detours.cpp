#include "detours.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace
{
	struct Pending
	{
		void** slot = nullptr;
		void*  detour = nullptr;
		void*  original = nullptr;
		bool   attach = true;
	};

	struct Live
	{
		void** slot = nullptr;
		void*  detour = nullptr;
		void*  trampoline = nullptr;
		uint8_t stolen[16]{};
		size_t  stolenLen = 0;
	};

	std::mutex              gLock;
	std::vector<Pending>    gQueue;
	std::vector<Live>       gLive;
	bool                    gOpen = false;

	constexpr size_t kPatchBytes = 14;

	uint8_t* WriteAbsJump(uint8_t* at, void* dest)
	{
		// jmp [rip+0]; dq dest
		at[0] = 0xFF;
		at[1] = 0x25;
		at[2] = 0x00;
		at[3] = 0x00;
		at[4] = 0x00;
		at[5] = 0x00;
		*reinterpret_cast<void**>(at + 6) = dest;
		return at + kPatchBytes;
	}

	void* MakeTrampoline(void* source, uint8_t* stolen, size_t stolenLen)
	{
		auto* block = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!block)
			return nullptr;

		memcpy(block, stolen, stolenLen);
		WriteAbsJump(block + stolenLen, static_cast<uint8_t*>(source) + stolenLen);
		return block;
	}

	bool Patch(void* at, const void* bytes, size_t len)
	{
		DWORD old = 0;
		if (!VirtualProtect(at, len, PAGE_EXECUTE_READWRITE, &old))
			return false;
		memcpy(at, bytes, len);
		VirtualProtect(at, len, old, &old);
		FlushInstructionCache(GetCurrentProcess(), at, len);
		return true;
	}

	bool ApplyAttach(Pending& item)
	{
		if (!item.slot || !*item.slot || !item.detour)
			return false;

		void* source = *item.slot;

		Live live{};
		live.slot = item.slot;
		live.detour = item.detour;
		live.stolenLen = kPatchBytes;
		memcpy(live.stolen, source, kPatchBytes);

		live.trampoline = MakeTrampoline(source, live.stolen, live.stolenLen);
		if (!live.trampoline)
			return false;

		uint8_t jump[kPatchBytes]{};
		WriteAbsJump(jump, item.detour);
		if (!Patch(source, jump, kPatchBytes))
		{
			VirtualFree(live.trampoline, 0, MEM_RELEASE);
			return false;
		}

		*item.slot = live.trampoline;
		gLive.push_back(live);
		return true;
	}

	bool ApplyDetach(Pending& item)
	{
		for (auto it = gLive.begin(); it != gLive.end(); ++it)
		{
			if (it->slot != item.slot)
				continue;

			Patch(*it->slot == it->trampoline ? item.original : *it->slot, it->stolen, it->stolenLen);
			// restore the real function pointer in the caller's slot
			void* real = static_cast<uint8_t*>(it->trampoline);
			// original address is trampoline's jump target minus stolen
			*it->slot = static_cast<uint8_t*>(it->trampoline); // will be overwritten
			// recover source from trampoline tail
			auto* tail = static_cast<uint8_t*>(it->trampoline) + it->stolenLen;
			void* source = *reinterpret_cast<void**>(tail + 6);
			source = static_cast<uint8_t*>(source) - it->stolenLen;
			Patch(source, it->stolen, it->stolenLen);
			*it->slot = source;
			VirtualFree(it->trampoline, 0, MEM_RELEASE);
			gLive.erase(it);
			return true;
		}
		return false;
	}
}

LONG WINAPI DetourTransactionBegin(VOID)
{
	std::lock_guard<std::mutex> lock(gLock);
	if (gOpen)
		return ERROR_INVALID_OPERATION;
	gQueue.clear();
	gOpen = true;
	return NO_ERROR;
}

LONG WINAPI DetourTransactionAbort(VOID)
{
	std::lock_guard<std::mutex> lock(gLock);
	gQueue.clear();
	gOpen = false;
	return NO_ERROR;
}

LONG WINAPI DetourUpdateThread(HANDLE)
{
	return NO_ERROR;
}

LONG WINAPI DetourAttach(PVOID* ppPointer, PVOID pDetour)
{
	std::lock_guard<std::mutex> lock(gLock);
	if (!gOpen || !ppPointer || !pDetour)
		return ERROR_INVALID_PARAMETER;
	Pending p{};
	p.slot = ppPointer;
	p.detour = pDetour;
	p.original = *ppPointer;
	p.attach = true;
	gQueue.push_back(p);
	return NO_ERROR;
}

LONG WINAPI DetourDetach(PVOID* ppPointer, PVOID pDetour)
{
	std::lock_guard<std::mutex> lock(gLock);
	if (!gOpen || !ppPointer)
		return ERROR_INVALID_PARAMETER;
	Pending p{};
	p.slot = ppPointer;
	p.detour = pDetour;
	p.original = *ppPointer;
	p.attach = false;
	gQueue.push_back(p);
	return NO_ERROR;
}

LONG WINAPI DetourTransactionCommit(VOID)
{
	std::lock_guard<std::mutex> lock(gLock);
	if (!gOpen)
		return ERROR_INVALID_OPERATION;

	LONG status = NO_ERROR;
	for (auto& item : gQueue)
	{
		const bool ok = item.attach ? ApplyAttach(item) : ApplyDetach(item);
		if (!ok)
			status = ERROR_INVALID_OPERATION;
	}

	gQueue.clear();
	gOpen = false;
	return status;
}
