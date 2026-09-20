#pragma once

#include "Pch.hpp"
#include "Patch.hpp"
#include "Arena.hpp"
#include "Gate.hpp"
#include "Satchel.hpp"
#include "Specs.hpp"
#include "Dance.hpp"
#include "Construct.hpp"
#include "Front.hpp"
#include "Bots.hpp"
#include "Settle.hpp"
#include "Wire.hpp"

namespace Revert::Router
{
	inline void (*PeOriginal)(UObject*, UFunction*, void*) = Rva::Rel<decltype(PeOriginal)>(Rva::ProcessEvent);
	inline void (*FlushOriginal)(UNetDriver*) = Rva::Rel<decltype(FlushOriginal)>(Rva::TickFlush);
	inline ENetMode (*NetOriginal)() = Rva::Rel<decltype(NetOriginal)>(Rva::NetMode);
	inline bool (*McpOriginal)() = Rva::Rel<decltype(McpOriginal)>(Rva::NoMcp);
	inline void (*DiedOriginal)(AFortPlayerControllerAthena*, FFortPlayerDeathReport) = Rva::Rel<decltype(DiedOriginal)>(Rva::ClientDied);
	inline void (*ReloadOriginal)(AFortWeapon*, int) = Rva::Rel<decltype(ReloadOriginal)>(Rva::ReloadCost);
	inline __int64 (*DispatchOriginal)(__int64, __int64*, int) = Rva::Rel<decltype(DispatchOriginal)>(Rva::DispatchRequest);

	inline bool LogCalls = false;

	inline bool OnExecute(AFortPlayerControllerAthena* pc, FGuid guid)
	{
		if (!pc)
			return true;

		auto* pawn = As<AFortPlayerPawnAthena>(pc->Pawn);
		if (!pawn)
			return true;

		UFortItemDefinition* def = nullptr;
		if (auto* item = Satchel::Locate(pc, guid))
			def = item->ItemEntry.ItemDefinition;
		else if (auto* row = Satchel::Replica(pc, guid))
			def = row->ItemDefinition;
		if (!def)
			return true;

		if (auto* gadget = As<UFortGadgetItemDefinition>(def))
			def = gadget->GetWeaponItemDefinition();
		if (!def)
			return true;

		if (def->IsA(UFortBuildingItemDefinition::StaticClass()))
		{
			if (auto* weapon = As<UFortWeaponItemDefinition>(def))
				pawn->EquipWeaponDefinition(weapon, guid);
			return true;
		}

		if (auto* deco = As<UFortDecoItemDefinition>(def))
		{
			pawn->PickUpActor(nullptr, deco);
			if (pawn->CurrentWeapon)
				pawn->CurrentWeapon->ItemEntryGuid = guid;
			if (auto* trap = As<AFortDecoTool_ContextTrap>(pawn->CurrentWeapon))
			{
				if (auto* context = As<UFortContextTrapItemDefinition>(def))
					trap->ContextTrapItemDefinition = context;
			}
			return true;
		}

		if (auto* weapon = As<UFortWeaponItemDefinition>(def))
			pawn->EquipWeaponDefinition(weapon, guid);
		return true;
	}

	inline bool OnDrop(AFortPlayerController* pc, FGuid guid, int count)
	{
		auto* item = pc ? Satchel::Locate(pc, guid) : nullptr;
		if (!item || !pc->MyFortPawn)
			return true;
		Satchel::Toss(item->ItemEntry.ItemDefinition, count, pc->MyFortPawn->K2_GetActorLocation(), item->ItemEntry.LoadedAmmo);
		Satchel::Strip(pc, guid, count);
		Satchel::Push(pc);
		return true;
	}

	inline bool OnPickup(AFortPlayerPawn* pawn, AFortPickup* pickup, float, FVector, bool)
	{
		auto* pc = pawn ? As<AFortPlayerController>(pawn->Controller) : nullptr;
		if (!pc || !pickup)
			return true;
		Satchel::Grant(pc, pickup->PrimaryPickupItemEntry.ItemDefinition, pickup->PrimaryPickupItemEntry.Count, pickup->PrimaryPickupItemEntry.LoadedAmmo);
		Satchel::Push(pc);
		pickup->K2_DestroyActor();
		return true;
	}

	inline void OnDied(AFortPlayerControllerAthena* pc, FFortPlayerDeathReport report)
	{
		static thread_local bool busy = false;
		if (busy)
			return;
		busy = true;
		Settle::HandleDeath(pc, report);
		DiedOriginal(pc, report);
		busy = false;
	}

	inline void OnReload(AFortWeapon* weapon, int spent)
	{
		auto* pawn = weapon ? As<AFortPawn>(weapon->GetOwner()) : nullptr;
		auto* pc = pawn ? As<AFortPlayerController>(pawn->Controller) : nullptr;

		if (pc && weapon)
		{
			if (auto* item = Satchel::Locate(pc, weapon->ItemEntryGuid))
			{
				item->ItemEntry.LoadedAmmo = weapon->AmmoCount;
				if (auto* row = Satchel::Replica(pc, weapon->ItemEntryGuid))
					row->LoadedAmmo = weapon->AmmoCount;
			}
			if (weapon->WeaponData && weapon->WeaponData->GetAmmoWorldItemDefinition_BP())
			{
				if (auto* ammo = Satchel::Locate(pc, weapon->WeaponData->GetAmmoWorldItemDefinition_BP()))
					Satchel::Strip(pc, ammo->ItemEntry.ItemGuid, spent);
			}
			Satchel::Push(pc);
		}
	}

	inline void OnPe(UObject* object, UFunction* function, void* params)
	{
		if (!object || !function)
			return;

		const auto name = function->GetName();
		if (LogCalls && name.find("Tick") == std::string::npos && name.find("Evaluate") == std::string::npos)
			Note("pe {} {}", object->GetName(), name);

		if (name == "OnSafeZoneStateChange")
		{
			if (!kStormRush)
			{
				PeOriginal(object, function, params);
				return;
			}
			return;
		}

		if (name == "K2_OnEndAbility")
		{
			Dance::OnAbilityEnded(object);
			PeOriginal(object, function, params);
			return;
		}

		if (name == "ServerPlayEmoteItem")
		{
			UFortMontageItemDefinitionBase* asset = nullptr;
			if (params)
				asset = static_cast<AFortPlayerController_ServerPlayEmoteItem_Params*>(params)->EmoteAsset;
			Dance::Play(As<AFortPlayerController>(object), asset);
			return;
		}

		if (name == "ServerAttemptAircraftJump")
		{
			auto* pc = As<AFortPlayerController>(object);
			FRotator look{};
			if (params)
				look = static_cast<AFortPlayerController_ServerAttemptAircraftJump_Params*>(params)->ClientRotation;
			Gate::JumpBus(pc, look);
			return;
		}

		if (kPlotMode && name == "TeleportPlayerToLinkedVolume")
		{
			auto* portal = As<AFortAthenaCreativePortal>(object);
			AFortPlayerPawn* pawn = nullptr;
			if (params)
				pawn = static_cast<AFortAthenaCreativePortal_TeleportPlayerToLinkedVolume_Params*>(params)->PlayerPawn;
			Island::EnterPlot(portal, pawn);
			return;
		}

		if (kPlotMode && name == "TeleportPlayerForPlotLoadComplete")
		{
			auto* portal = As<AFortAthenaCreativePortal>(object);
			AFortPlayerPawn* pawn = nullptr;
			if (params)
				pawn = static_cast<AFortAthenaCreativePortal_TeleportPlayerForPlotLoadComplete_Params*>(params)->PlayerPawn;
			auto* owner = pawn ? As<AFortPlayerControllerAthena>(pawn->Controller) : nullptr;
			if (portal && owner && owner->CreativePlotLinkedVolume && owner->CreativePlotLinkedVolume == portal->LinkedVolume)
				Island::EnterPlot(portal, pawn);
			return;
		}

		if (kPlotMode && name == "ServerTeleportToPlaygroundLobbyIsland")
		{
			Island::ReturnToLobby(As<AFortPlayerControllerAthena>(object));
			return;
		}

		if (name == "ClientReturnToMainMenu")
		{
			if (params)
			{
				auto* home = static_cast<APlayerController_ClientReturnToMainMenu_Params*>(params);
				Arena::FillHomeReason(home->ReturnReason);
			}
			PeOriginal(object, function, params);
			return;
		}

		if (name == "ServerReturnToMainMenu")
		{
			Arena::SendHome(As<APlayerController>(object));
			return;
		}

		if (name == "K2_OnLogout")
		{
			AController* exiting = nullptr;
			if (params)
				exiting = static_cast<AGameModeBase_K2_OnLogout_Params*>(params)->ExitingController;
			Arena::NoteLogout(exiting);
			PeOriginal(object, function, params);
			return;
		}

		PeOriginal(object, function, params);
	}

	inline void OnFlush(UNetDriver* driver)
	{
		if (!Arena::Reloading)
		{
			if (!Arena::ReloadQueued)
			{
				Front::Pulse();
				if (kSolo)
					Bots::TickAll();
				Settle::Pulse();
			}
			Arena::TickReload();
		}
		using RepFn = void (*)(UReplicationDriver*);
		static auto Rep = Rva::Rel<RepFn>(Rva::ReplicateActors);
		if (!Arena::Reloading && driver && driver->ReplicationDriver)
			Rep(driver->ReplicationDriver);
		FlushOriginal(driver);
		if (Arena::KillQueued)
		{
			Wire::Drain();
			Arena::KillGs();
		}
	}

	inline ENetMode OnNet()
	{
		return NM_DedicatedServer;
	}

	inline bool OnMcp()
	{
		return !Cfg::Mcp;
	}

	inline float OnTick(UGameEngine*, float, bool)
	{
		return static_cast<float>(Cfg::MaxTickRate);
	}

	inline void OnKick(AGameSession*, AController*)
	{
	}

	inline __int64 OnDispatch(__int64 a, __int64* b, int)
	{
		return DispatchOriginal(a, b, Cfg::McpDedicated);
	}

	inline void BindAll()
	{
		auto* modeCdo = AFortGameModeAthena::StaticClass()->CreateDefaultObject();
		auto* pcCdo = Find<AFortPlayerControllerAthena>("/Game/Athena/Athena_PlayerController.Default__Athena_PlayerController_C");
		auto* pawnCdo = Find<AFortPlayerPawnAthena>("/Game/Athena/PlayerPawn_Athena.Default__PlayerPawn_Athena_C");
		auto* ascCdo = UFortAbilitySystemComponentAthena::StaticClass()->CreateDefaultObject();
		auto* psCdo = AFortPlayerStateAthena::StaticClass()->CreateDefaultObject();
		auto* broadcastCdo = AFortBroadcastRemoteClientInfo::StaticClass()->CreateDefaultObject();

		Patch::BindExec(modeCdo, Find<UFunction>("/Script/Engine.GameMode.ReadyToStartMatch"), Arena::OnReady, reinterpret_cast<void**>(&Arena::ReadyOriginal));
		Patch::BindExec(modeCdo, Find<UFunction>("/Script/Engine.GameModeBase.HandleStartingNewPlayer"), Gate::Admit, reinterpret_cast<void**>(&Gate::AdmitOriginal));
		Patch::BindExec(modeCdo, Find<UFunction>("/Script/Engine.GameModeBase.SpawnDefaultPawnFor"), Gate::SpawnFor);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/Engine.PlayerController.ServerAcknowledgePossession"), Gate::PossessAck);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerExecuteInventoryItem"), OnExecute);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerPlayEmoteItem"), Dance::Play);
		Patch::BindExec(pawnCdo, Find<UFunction>("/Script/FortniteGame.FortPawn.MovingEmoteStopped"), Dance::MovingStopped);
		Patch::BindExec(pawnCdo, Find<UFunction>("/Script/FortniteGame.FortPawn.EmoteStopped"), Dance::EmoteStopped, reinterpret_cast<void**>(&Dance::EmoteStoppedOriginal));
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerAttemptInventoryDrop"), OnDrop);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerCreateBuildingActor"), Construct::Place);
		Patch::BindExec(broadcastCdo, Find<UFunction>("/Script/FortniteGame.FortBroadcastRemoteClientInfo.ServerSetPlayerBuildableClass"), Construct::RememberClass);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerBeginEditingBuildingActor"), Construct::BeginEdit);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerEndEditingBuildingActor"), Construct::EndEdit);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerEditBuildingActor"), Construct::Edit);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerRepairBuildingActor"), Construct::Repair);
		Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerLoadingScreenDropped"), Gate::ScreenDropped, reinterpret_cast<void**>(&Gate::ScreenOriginal), 0x12F0);
		Patch::BindExec(ascCdo, Find<UFunction>("/Script/GameplayAbilities.AbilitySystemComponent.ServerTryActivateAbility"), Specs::OnTry);
		Patch::BindExec(ascCdo, Find<UFunction>("/Script/GameplayAbilities.AbilitySystemComponent.ServerTryActivateAbilityWithEventData"), Specs::OnTryEvent);
		Patch::BindExec(pawnCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerPawn.ServerHandlePickup"), OnPickup);

		if (kStormRush)
			Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerController.ServerAttemptAircraftJump"), Gate::JumpBus);

		if (kPlotMode)
			Patch::BindExec(pcCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerControllerAthena.ServerTeleportToPlaygroundLobbyIsland"), Island::ReturnToLobby);

		Patch::BindExec(psCdo, Find<UFunction>("/Script/FortniteGame.FortPlayerStateAthena.ServerSetInAircraft"), Gate::InAircraft, reinterpret_cast<void**>(&Gate::AircraftOriginal));
		Front::BoardAlive = Gate::BoardAlive;
		Front::FlushBus = Gate::FlushBus;
		Front::AnyoneInBus = Gate::AnyoneInBus;
		Arena::FirstJoinPtr = &Gate::FirstJoin;
		Arena::OnRoundReset = []()
		{
			Bots::ResetAll();
			Settle::Reset();
		};

		Patch::Attach(reinterpret_cast<void**>(&Construct::DamageOriginal), reinterpret_cast<void*>(&Construct::OnBuildingDamage));
		Patch::Attach(reinterpret_cast<void**>(&PeOriginal), OnPe);
		Patch::Attach(reinterpret_cast<void**>(&FlushOriginal), OnFlush);
		Patch::Attach(reinterpret_cast<void**>(&NetOriginal), OnNet);
		Patch::Attach(reinterpret_cast<void**>(&McpOriginal), OnMcp);

		auto gc = Rva::Ptr(Rva::CollectGarbage);
		auto quiet = reinterpret_cast<void*>(&Patch::Quiet);
		Patch::Attach(&gc, quiet);

		auto kick = Rva::Ptr(Rva::KickPlayer);
		auto nokick = reinterpret_cast<void*>(&OnKick);
		Patch::Attach(&kick, nokick);

		auto tick = Rva::Ptr(Rva::MaxTick);
		auto tickFn = reinterpret_cast<void*>(&OnTick);
		Patch::Attach(&tick, tickFn);

		auto team = Rva::Ptr(Rva::PickTeam);
		auto teamFn = reinterpret_cast<void*>(&Gate::TeamPick);
		Patch::Attach(&team, teamFn);

		auto can = Rva::Ptr(Rva::CanActivate);
		auto canFn = reinterpret_cast<void*>(&Patch::Yes);
		Patch::Attach(&can, canFn);

		Patch::Attach(reinterpret_cast<void**>(&DiedOriginal), reinterpret_cast<void*>(&OnDied));
		Patch::Attach(reinterpret_cast<void**>(&ReloadOriginal), reinterpret_cast<void*>(&OnReload));
		Patch::Attach(reinterpret_cast<void**>(&DispatchOriginal), reinterpret_cast<void*>(&OnDispatch));

		if (kSolo)
			Bots::Bind();

		Note("binds installed");
		Note("mcp {}  {}:{}{}  key {}", Cfg::Mcp ? "on" : "off", Cfg::McpHost, Cfg::McpPort, Cfg::McpHttps ? " https" : "", (Cfg::McpApiKey && *Cfg::McpApiKey) ? "on" : "off");
	}
}
