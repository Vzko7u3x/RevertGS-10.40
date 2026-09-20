#pragma once

#include <cstdint>
#include <Windows.h>

namespace Revert::Rva
{
	inline uintptr_t Base()
	{
		return reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
	}

	template <typename T>
	inline T Rel(uintptr_t rva)
	{
		return reinterpret_cast<T>(Base() + rva);
	}

	inline void* Ptr(uintptr_t rva)
	{
		return Rel<void*>(rva);
	}

	constexpr uintptr_t Objects          = 0x64A0090;
	constexpr uintptr_t Engine           = 0x65A40A0;
	constexpr uintptr_t ClientFlag       = 0x637925B;
	constexpr uintptr_t ServerFlag       = 0x637925C;
	constexpr uintptr_t ProcessEvent     = 0x22F2990;
	constexpr uintptr_t NetMode          = 0x34D2140;
	constexpr uintptr_t NoMcp            = 0x161D600;
	constexpr uintptr_t TickFlush        = 0x31EECB0;
	constexpr uintptr_t CreateNetDriver  = 0x347FAF0;
	constexpr uintptr_t InitListen       = 0x6F5F90;
	constexpr uintptr_t SetWorld         = 0x31EDF40;
	constexpr uintptr_t ShutdownNet      = 0x348FB90;
	constexpr uintptr_t ReplicateActors  = 0xA33E90;
	constexpr uintptr_t GiveAbility      = 0x935010;
	constexpr uintptr_t GiveAbilityOnce  = 0x935130;
	constexpr uintptr_t AbilitySpecCtor  = 0x958F90;
	constexpr uintptr_t ClearAbility     = 0x9233D0;
	constexpr uintptr_t TryActivate      = 0x9367F0;
	constexpr uintptr_t CollectGarbage   = 0x227D720;
	constexpr uintptr_t KickPlayer       = 0x17F07B0;
	constexpr uintptr_t ClientDied       = 0x1F34E50;
	constexpr uintptr_t SpawnLoot        = 0x13A91C0;
	constexpr uintptr_t PickTeam         = 0x11D42B0;
	constexpr uintptr_t MaxTick          = 0x3085220;
	constexpr uintptr_t ReloadCost       = 0x1C66A30;
	constexpr uintptr_t PickupDelay      = 0x16F7D10;
	constexpr uintptr_t CanActivate      = 0x9214C0;
	constexpr uintptr_t DispatchRequest  = 0xBAED60;
	constexpr uintptr_t ViewPoint        = 0x19A4780;
	constexpr uintptr_t CapsuleOverlap   = 0x196DB00;
	constexpr uintptr_t BuildingDamage   = 0x136A8B0;
	constexpr uintptr_t CantBuild        = 0x1601820;
	constexpr uintptr_t ReplaceBuilding  = 0x13D0DE0;
	constexpr uintptr_t SquadId          = 0x17DDBB0;
	constexpr uintptr_t MatchStartTime   = 0x17F9660;
	constexpr uintptr_t ContextCreate    = 0x22A30C0;
	constexpr uintptr_t ArrayGet         = 0x312BBE0;
	constexpr uintptr_t Resurrection     = 0x12147A0;
	constexpr uintptr_t RebootTeam       = 0x1243CB0;
	constexpr uintptr_t LoadPlayset      = 0x1A3A0A0;
	constexpr uintptr_t SpawnBot         = 0x12B1C50;
	constexpr uintptr_t AircraftExit     = 0x11CF710;
	constexpr uintptr_t AircraftEnter    = 0x11CF670;
	constexpr uintptr_t RemoveAlive      = 0x11D95E0;
}

namespace Revert
{
	inline void MarkDedicated()
	{
		*Rva::Rel<bool*>(Rva::ClientFlag) = false;
		*Rva::Rel<bool*>(Rva::ServerFlag) = true;
	}

	inline void MarkTravel()
	{
		*Rva::Rel<bool*>(Rva::ClientFlag) = true;
		*Rva::Rel<bool*>(Rva::ServerFlag) = false;
	}
}
