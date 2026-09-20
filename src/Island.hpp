#pragma once

#include "Pch.hpp"
#include "Satchel.hpp"

namespace Revert::Island
{
	using LoadPlaysetFn = __int64 (*)(UPlaysetLevelStreamComponent*);
	inline LoadPlaysetFn LoadPlayset = Rva::Rel<LoadPlaysetFn>(Rva::LoadPlayset);

	inline constexpr float kPlotFloorZ = -3000.f;

	inline void WakeFoundation(ABuildingFoundation* slab)
	{
		if (!slab)
			return;
		slab->DynamicFoundationType = EDynamicFoundationType::StartEnabled_Stationary;
		slab->bServerStreamedInLevel = true;
		slab->OnRep_ServerStreamedInLevel();
		slab->DynamicFoundationRepData.EnabledState = EDynamicFoundationEnabledState::Enabled;
		slab->OnRep_DynamicFoundationRepData();
		slab->SetDynamicFoundationEnabled(true);
	}

	inline void ApplyPlayset(UFortPlaysetItemDefinition* playset, AFortVolume* volume)
	{
		if (!playset || !volume)
			return;

		volume->OverridePlayset = playset;
		volume->CurrentPlayset = playset;
		volume->SetCurrentPlayset(playset);
		volume->OnRep_CurrentPlayset();

		if (auto* stream = static_cast<UPlaysetLevelStreamComponent*>(volume->GetComponentByClass(UPlaysetLevelStreamComponent::StaticClass())))
		{
			stream->SetPlayset(playset);
			stream->OnRep_ClientPlaysetData();
			LoadPlayset(stream);
		}
	}

	inline bool IsLobbySpawn(AActor* actor)
	{
		auto* start = As<AFortPlayerStartCreative>(actor);
		if (!start || start->K2_GetActorLocation().Z < kPlotFloorZ)
			return false;
		if (start->PlayerStartTags.Contains("Playground.LobbyIsland.Spawn"))
			return true;
		const auto tags = start->PlayerStartTags.ToStringSimple(false);
		return tags.find("LobbyIsland") != std::string::npos;
	}

	inline void DumpStartsOnce()
	{
		static bool dumped = false;
		if (dumped)
			return;
		dumped = true;

		TArray<AActor*> found;
		UGameplayStatics::GetAllActorsOfClass(World(), AFortPlayerStartCreative::StaticClass(), &found);
		Note("creative starts {}", found.Num());
		const int limit = found.Num() < 12 ? found.Num() : 12;
		for (int i = 0; i < limit; ++i)
		{
			auto* start = As<AFortPlayerStartCreative>(found[i]);
			if (!start)
				continue;
			const auto loc = start->K2_GetActorLocation();
			Note("start {} tags [{}] island={} z={:.0f}", start->GetName(), start->PlayerStartTags.ToStringSimple(false), start->bUseAsIslandStart, loc.Z);
		}
		found.Free();
	}

	inline AActor* LobbyStart(AController* pc = nullptr)
	{
		TArray<AActor*> found;
		UGameplayStatics::GetAllActorsOfClass(World(), AFortPlayerStartCreative::StaticClass(), &found);

		AActor* tagged = nullptr;
		AActor* taggedOff = nullptr;
		AActor* high = nullptr;
		for (int i = 0; i < found.Num(); ++i)
		{
			auto* start = As<AFortPlayerStartCreative>(found[i]);
			if (!start)
				continue;

			if (IsLobbySpawn(start))
			{
				if (start->bIsEnabled)
				{
					tagged = start;
					break;
				}
				if (!taggedOff)
					taggedOff = start;
				continue;
			}

			if (!high && start->K2_GetActorLocation().Z > kPlotFloorZ)
				high = start;
		}
		found.Free();

		if (tagged)
			return tagged;
		if (taggedOff)
			return taggedOff;

		if (pc)
		{
			if (auto* mode = Mode())
			{
				if (auto* pick = mode->ChoosePlayerStart(pc))
				{
					if (IsLobbySpawn(pick))
						return pick;
				}
			}
		}

		return high;
	}

	inline UFortWeaponItemDefinition* PhoneDef()
	{
		return Find<UFortWeaponItemDefinition>("/Game/Athena/Items/Weapons/Prototype/WID_CreativeTool.WID_CreativeTool");
	}

	inline void EnableCreativeTools(AFortPlayerControllerAthena* pc, bool on)
	{
		if (!pc)
			return;

		const bool wasQuick = pc->IsCreativeQuickbarEnabled();
		pc->bIsCreativeQuickbarEnabled = on;
		pc->OnRep_IsCreativeQuickbarEnabled(wasQuick);
		pc->bIsCreativeQuickmenuEnabled = on;
		pc->SetIsCreativeModeEnabled(on);
		pc->OnRep_IsCreativeModeEnabled();
	}

	inline void UseLobbyVolume(AFortPlayerControllerAthena* pc)
	{
		if (!pc)
			return;
		EnableCreativeTools(pc, false);
		pc->CreativePlotLinkedVolume = pc->GetCurrentVolume();
		pc->OnRep_CreativePlotLinkedVolume();
	}

	inline void GivePhone(AFortPlayerControllerAthena* pc)
	{
		if (!pc)
			return;
		if (auto* phone = PhoneDef())
		{
			if (!Satchel::Locate(pc, phone))
			{
				Satchel::Grant(pc, phone, 1);
				pc->ClientCreativePhoneCreated();
				Satchel::Push(pc);
			}
		}
		EnableCreativeTools(pc, true);
	}

	inline void TakePhone(AFortPlayerControllerAthena* pc)
	{
		if (!pc)
			return;
		if (auto* phone = PhoneDef())
		{
			if (auto* item = Satchel::Locate(pc, phone))
				Satchel::Strip(pc, item->ItemEntry.ItemGuid, item->ItemEntry.Count);
			Satchel::Push(pc);
		}
		EnableCreativeTools(pc, false);
	}

	inline bool OnOwnPlot(AFortPlayerControllerAthena* pc)
	{
		return pc && pc->OwnedPortal && pc->OwnedPortal->LinkedVolume && pc->CreativePlotLinkedVolume == pc->OwnedPortal->LinkedVolume;
	}

	inline void ClaimPortal(AFortPlayerControllerAthena* pc)
	{
		auto* state = State();
		auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState);
		if (!state || !state->CreativePortalManager || !ps || !state->CreativePortalManager->AvailablePortals.Num())
			return;

		auto* portal = state->CreativePortalManager->AvailablePortals[0];
		state->CreativePortalManager->AvailablePortals.Remove(0);
		state->CreativePortalManager->UsedPortals.Add(portal);

		portal->OwningPlayer = ps->UniqueId;
		portal->OnRep_OwningPlayer();
		portal->bPortalOpen = true;
		portal->OnRep_PortalOpen();
		portal->PlayersReady.Add(portal->OwningPlayer);
		portal->OnRep_PlayersReady();
		portal->bUserInitiatedLoad = true;
		portal->bInErrorState = false;
		portal->IslandInfo.CreatorName = ps->GetPlayerName();
		portal->OnRep_IslandInfo();

		pc->OwnedPortal = portal;

		if (auto* volume = portal->LinkedVolume)
		{
			volume->bNeverAllowSaving = false;
			volume->VolumeState = EVolumeState::Ready;
			volume->OnRep_VolumeState();

			if (auto* save = static_cast<UFortLevelSaveComponent*>(volume->GetComponentByClass(UFortLevelSaveComponent::StaticClass())))
			{
				save->AccountIdOfOwner = ps->UniqueId;
				save->bIsLoaded = true;
				save->LoadedPlotInstanceId = L"1";
				save->OnRep_LoadedPlotInstanceId();
				save->OnRep_LoadedLinkData();

				if (auto* plot = Find<UFortCreativeRealEstatePlotItemDefinition>("/Game/Playgrounds/Items/Plots/Temperate_Medium.Temperate_Medium"))
				{
					save->RestrictedPlotDefinition = plot;
					save->bAutoLoadFromRestrictedPlotDefinition = true;
					save->bLoadPlaysetFromPlot = true;
				}
			}

			if (auto* playset = Find<UFortPlaysetItemDefinition>("/Game/Playsets/PID_Playset_60x60_Composed.PID_Playset_60x60_Composed"))
			{
				volume->SetCurrentPlayset(playset);
				ApplyPlayset(playset, volume);
			}
		}

		UseLobbyVolume(pc);
	}

	inline void LandPlayer(AFortPlayerControllerAthena* pc)
	{
		if (!pc || OnOwnPlot(pc))
			return;

		auto* start = LobbyStart(pc);
		if (!start)
		{
			DumpStartsOnce();
			Note("creative lobby start missing");
			UseLobbyVolume(pc);
			return;
		}

		const FVector loc = start->K2_GetActorLocation();
		const FRotator rot = start->K2_GetActorRotation();

		auto* pawn = As<AFortPlayerPawnAthena>(pc->Pawn ? pc->Pawn : pc->MyFortPawn);
		if (!pawn)
		{
			if (auto* mode = Mode())
			{
				mode->RestartPlayerAtPlayerStart(pc, start);
				pawn = As<AFortPlayerPawnAthena>(pc->Pawn);
			}
		}
		if (!pawn && World())
		{
			static auto* pawnClass = Find<UBlueprintGeneratedClass>("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
			pawn = World()->SpawnActor<AFortPlayerPawnAthena>(loc, rot, pawnClass);
			if (pawn)
				pc->Possess(pawn);
		}
		if (!pawn)
		{
			Note("creative spawn failed");
			return;
		}

		pc->bAutoManageActiveCameraTarget = true;
		pc->ResetIgnoreMoveInput();
		pc->ResetIgnoreLookInput();
		pc->ClientGotoState(MakeName(L"Playing"));
		pawn->K2_TeleportTo(loc, rot);

		pawn->SetActorHiddenInGame(false);
		pawn->SetActorEnableCollision(true);
		pc->MyFortPawn = pawn;
		pc->AcknowledgedPawn = pawn;
		FViewTargetTransitionParams blend{};
		pc->ClientSetViewTarget(pawn, blend);
		UseLobbyVolume(pc);
		Note("creative lobby {:.0f} {:.0f} {:.0f}", loc.X, loc.Y, loc.Z);
	}

	inline AFortPlayerPawn* PlotPawn(AFortAthenaCreativePortal* portal, AFortPlayerPawn* pawn)
	{
		if (pawn)
			return pawn;
		if (!portal)
			return nullptr;
		if (auto* mode = Mode())
		{
			for (int i = 0; i < mode->AlivePlayers.Num(); ++i)
			{
				auto* pc = mode->AlivePlayers[i];
				if (pc && pc->OwnedPortal == portal)
					return As<AFortPlayerPawn>(pc->Pawn ? pc->Pawn : pc->MyFortPawn);
			}
		}
		return nullptr;
	}

	inline void EnterPlot(AFortAthenaCreativePortal* portal, AFortPlayerPawn* incoming)
	{
		auto* pawn = PlotPawn(portal, incoming);
		if (!portal || !pawn || !portal->LinkedVolume)
			return;

		auto* pc = As<AFortPlayerControllerAthena>(pawn->Controller);
		if (!pc)
			return;

		GivePhone(pc);
		pc->CreativePlotLinkedVolume = portal->LinkedVolume;
		pc->OnRep_CreativePlotLinkedVolume();

		auto loc = portal->LinkedVolume->K2_GetActorLocation();
		loc.Z = 10000.f;
		pawn->K2_TeleportTo(loc, {});
		pawn->BeginSkydiving(false);
		Note("creative plot {:.0f} {:.0f} {:.0f}", loc.X, loc.Y, loc.Z);
	}

	inline void ReturnToLobby(AFortPlayerControllerAthena* pc)
	{
		if (!pc)
			return;

		TakePhone(pc);
		auto* start = LobbyStart(pc);
		if (!start)
		{
			if (auto* mode = Mode())
			{
				if (auto* pick = mode->ChoosePlayerStart(pc); IsLobbySpawn(pick) || (pick && pick->K2_GetActorLocation().Z > kPlotFloorZ))
					start = pick;
			}
		}

		if (auto* pawn = pc->Pawn ? pc->Pawn : pc->MyFortPawn)
		{
			if (start)
				pawn->K2_TeleportTo(start->K2_GetActorLocation(), start->K2_GetActorRotation());
		}

		UseLobbyVolume(pc);
		if (start)
		{
			const auto loc = start->K2_GetActorLocation();
			Note("creative lobby return {:.0f} {:.0f} {:.0f}", loc.X, loc.Y, loc.Z);
		}
		else
			Note("creative lobby return (no start)");
	}
}
