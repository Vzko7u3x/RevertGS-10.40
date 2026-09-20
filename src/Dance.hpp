#pragma once

#include "Pch.hpp"
#include "Specs.hpp"

namespace Revert::Dance
{
	inline void (*EmoteStoppedOriginal)(AFortPawn*, UFortItemDefinition*) = nullptr;
	inline AFortPlayerController* LastPc = nullptr;
	inline float LastPlayAt = 0.f;

	inline bool PathUsable(const std::string& path)
	{
		if (path.empty() || path == "None" || path == "none")
			return false;
		if (path.find('/') == std::string::npos && path.find("None") != std::string::npos)
			return false;
		return true;
	}

	inline UGameplayAbility* AsAbility(UObject* object)
	{
		if (!object)
			return nullptr;

		const auto name = object->GetName();
		if (name.find("ClientPilot") != std::string::npos)
			return nullptr;

		if (auto* ability = As<UGameplayAbility>(object))
			return ability;

		if (auto* klass = As<UClass>(object))
			return As<UGameplayAbility>(klass->CreateDefaultObject());

		return nullptr;
	}

	inline UGameplayAbility* AbilityCdo(const char* path)
	{
		if (!path || !PathUsable(path))
			return nullptr;

		if (auto* ability = AsAbility(UObject::FindObject<UGameplayAbility>(path)))
			return ability;

		if (auto* klass = UObject::FindObject<UClass>(path))
			return AsAbility(klass->CreateDefaultObject());

		if (auto* klass = UObject::FindClass(path))
			return AsAbility(klass->CreateDefaultObject());

		return nullptr;
	}

	inline UGameplayAbility* AbilityFromSoftPath(FSoftObjectPath path)
	{
		if (!path.AssetPathName.IsValid())
			return nullptr;

		auto name = path.AssetPathName;
		const auto text = name.ToString();
		if (!PathUsable(text))
			return nullptr;

		return AbilityCdo(text.c_str());
	}

	inline UGameplayAbility* GenericDance()
	{
		static UGameplayAbility* cached = nullptr;
		if (cached)
			return cached;

		cached = AbilityCdo("/Game/Abilities/Emotes/GAB_Emote_Generic.Default__GAB_Emote_Generic_C");
		if (!cached)
			cached = AbilityCdo("/Game/Abilities/Emotes/GAB_Emote_Generic.GAB_Emote_Generic_C");
		return cached;
	}

	inline UGameplayAbility* GenericSpray()
	{
		static UGameplayAbility* cached = nullptr;
		if (cached)
			return cached;

		cached = AbilityCdo("/Game/Abilities/Sprays/GAB_Spray_Generic.Default__GAB_Spray_Generic_C");
		if (!cached)
			cached = AbilityCdo("/Game/Abilities/Sprays/GAB_Spray_Generic.GAB_Spray_Generic_C");
		return cached;
	}

	inline void Finish(AFortPawn* pawn)
	{
		if (!pawn)
			return;
		pawn->bMovingEmote = false;
		pawn->bMovingEmoteForwardOnly = false;
		pawn->LastEmoteTime = 0.f;
		pawn->LastEmoteEndTime = 0.f;
		if (auto* player = As<AFortPlayerPawn>(pawn))
			player->bIsPlayingEmote = 0;
		if (pawn->LastReplicatedEmoteExecuted)
		{
			pawn->LastReplicatedEmoteExecuted = nullptr;
			pawn->OnRep_LastReplicatedEmoteExecuted();
		}
	}

	inline bool TooSoon(AFortPlayerController* pc)
	{
		auto* world = World();
		const float now = world ? UGameplayStatics::GetTimeSeconds(world) : 0.f;
		if (LastPc == pc && now - LastPlayAt < 0.2f)
			return true;
		LastPc = pc;
		LastPlayAt = now;
		return false;
	}

	inline void Play(AFortPlayerController* pc, UFortMontageItemDefinitionBase* asset)
	{
		if (!pc || !asset || TooSoon(pc))
			return;

		auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState);
		auto* pawn = As<AFortPawn>(pc->MyFortPawn ? pc->MyFortPawn : pc->Pawn);
		if (!ps || !ps->AbilitySystemComponent || !pawn)
			return;

		UGameplayAbility* ability = nullptr;
		if (As<UAthenaSprayItemDefinition>(asset))
		{
			ability = GenericSpray();
		}
		else if (auto* toy = As<UAthenaToyItemDefinition>(asset))
		{
			ability = AbilityFromSoftPath(toy->ToySpawnAbility.ObjectID);
		}

		if (!ability)
		{
			if (auto* dance = As<UAthenaDanceItemDefinition>(asset))
			{
				pawn->bMovingEmote = dance->bMovingEmote;
				pawn->bMovingEmoteForwardOnly = dance->bMoveForwardOnly;
				pawn->EmoteWalkSpeed = dance->WalkForwardSpeed;

				auto* custom = reinterpret_cast<TSoftObjectPtr<UClass>*>(reinterpret_cast<uint8_t*>(dance) + 0x04E0);
				ability = AbilityFromSoftPath(custom->ObjectID);
			}
			if (!ability)
				ability = GenericDance();
		}

		if (!ability)
		{
			Note("emote ability missing for {}", asset->GetName());
			return;
		}

		Note("emote play {} via {}", asset->GetName(), ability->GetName());
		Finish(pawn);
		Specs::ActivateOnce(ps->AbilitySystemComponent, ability, asset);
		pawn->LastEmoteItemDef = asset;
		pawn->LastReplicatedEmoteExecuted = asset;
		pawn->OnRep_LastReplicatedEmoteExecuted();
	}

	inline void MovingStopped(AFortPawn* pawn)
	{
		Finish(pawn);
	}

	inline void EmoteStopped(AFortPawn* pawn, UFortItemDefinition* montage)
	{
		Finish(pawn);
		if (EmoteStoppedOriginal)
			EmoteStoppedOriginal(pawn, montage);
	}

	inline bool IsEmoteAbility(UObject* object)
	{
		if (!object)
			return false;
		const auto name = object->GetName();
		return name.find("Emote") != std::string::npos
			|| name.find("Spray") != std::string::npos
			|| name.find("Toy") != std::string::npos;
	}

	inline void OnAbilityEnded(UObject* object)
	{
		if (!IsEmoteAbility(object))
			return;
		if (auto* ga = As<UFortGameplayAbility>(object))
			Finish(ga->GetActivatingPawn());
	}
}
