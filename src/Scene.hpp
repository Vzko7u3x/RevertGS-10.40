#pragma once

#include "Image.hpp"
#include "Log.hpp"

enum ENetMode
{
	NM_Standalone,
	NM_DedicatedServer,
	NM_ListenServer,
	NM_Client,
	NM_MAX,
};

namespace Revert
{
	template <typename T>
	inline T* As(UObject* object)
	{
		if (object && object->IsA(T::StaticClass()))
			return static_cast<T*>(object);
		return nullptr;
	}

	inline UFortEngine* Engine()
	{
		return *Rva::Rel<UFortEngine**>(Rva::Engine);
	}

	inline UWorld* World()
	{
		auto* engine = Engine();
		return (engine && engine->GameViewport) ? engine->GameViewport->World : nullptr;
	}

	inline AFortGameModeAthena* Mode()
	{
		auto* world = World();
		return world ? As<AFortGameModeAthena>(world->AuthorityGameMode) : nullptr;
	}

	inline AFortGameStateAthena* State()
	{
		auto* world = World();
		return world ? As<AFortGameStateAthena>(world->GameState) : nullptr;
	}

	inline FName MakeName(const wchar_t* text)
	{
		return UKismetStringLibrary::Conv_StringToName(text);
	}

	template <typename T>
	inline T* Find(const char* path)
	{
		if (auto* hit = UObject::FindObject<T>(path))
			return hit;

		const char* leaf = path;
		for (const char* cursor = path; *cursor; ++cursor)
		{
			if (*cursor == '.')
				leaf = cursor + 1;
		}
		return (leaf && *leaf) ? UObject::FindObjectSlow<T>(leaf) : nullptr;
	}

	template <typename T>
	inline T* FirstOf(UClass* type = T::StaticClass())
	{
		TArray<AActor*> found;
		UGameplayStatics::GetAllActorsOfClass(World(), type, &found);
		T* result = found.Num() ? As<T>(found[0]) : nullptr;
		found.Free();
		return result;
	}

	inline int RandRange(int lo, int hi)
	{
		if (hi <= lo)
			return lo;
		return lo + (std::rand() % (hi - lo + 1));
	}
}
