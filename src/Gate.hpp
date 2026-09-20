#pragma once

#include "Pch.hpp"
#include "Satchel.hpp"
#include "Specs.hpp"
#include "Island.hpp"
#include "Front.hpp"
#include "Flavor.hpp"
#include "Arena.hpp"
#include "Patch.hpp"
#include "Wire.hpp"

namespace Revert::Gate
{
	inline void (*AdmitOriginal)(AFortGameModeAthena*, AFortPlayerControllerAthena*) = nullptr;
	inline void (*ScreenOriginal)(AFortPlayerControllerAthena*) = nullptr;
	inline void (*AircraftOriginal)(AFortPlayerStateAthena*, bool) = nullptr;
	inline bool FirstJoin = true;

	inline bool ViewingBus(AFortPlayerControllerAthena* pc, AFortAthenaAircraft* bus);

	inline void Paint(AFortPlayerController* pc, AFortPlayerPawnAthena* pawn)
	{
		if (!pc || !pawn)
			return;
		pawn->CosmeticLoadout = pc->CosmeticLoadoutPC;
		pawn->OnRep_CosmeticLoadout();
		if (auto* ps = As<AFortPlayerState>(pc->PlayerState))
			UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization(ps);
	}

	inline void Dress(AFortPlayerControllerAthena* pc, AFortPlayerState* ps)
	{
		if (!pc->CosmeticLoadoutPC.Character)
		{
			pc->CosmeticLoadoutPC.Character = Find<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_001_Athena_Commando_F_Default.CID_001_Athena_Commando_F_Default");
			pc->CosmeticLoadoutPC.bIsDefaultCharacter = true;
		}
		Satchel::BindPickaxe(pc);

		if (!pc->CosmeticLoadoutPC.Character || !pc->CosmeticLoadoutPC.Character->HeroDefinition)
			return;

		ps->HeroType = pc->CosmeticLoadoutPC.Character->HeroDefinition;
		ps->OnRep_HeroType();
		UFortKismetLibrary::UpdatePlayerCustomCharacterPartsVisualization(ps);
	}

	inline void Admit(AFortGameModeAthena* mode, AFortPlayerControllerAthena* pc)
	{
		if (!pc)
			return;

		auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState);
		if (!ps)
			return;

		Arena::KeepPlaylist();

		if (kPlotMode)
		{
			FirstJoin = false;
			Arena::HoldCreative();
		}
		else if (FirstJoin && Arena::CanLaunchBus())
		{
			FirstJoin = false;
			Arena::LaunchBus();
		}
		Arena::HadHumans = true;

		pc->bHasServerFinishedLoading = true;
		pc->OnRep_bHasServerFinishedLoading();
		ps->bHasStartedPlaying = true;
		ps->OnRep_bHasStartedPlaying();

		Dress(pc, ps);
		Specs::SeedPlayer(ps);

		if (ps->PlayerTeam)
		{
			ps->SquadId = static_cast<uint8_t>(ps->TeamIndex - 2);
			FGameMemberInfo member{};
			member.TeamIndex = ps->TeamIndex;
			member.SquadId = ps->SquadId;
			member.MemberUniqueId = ps->UniqueId;
			State()->GameMemberInfoArray.Members.Add(member);
			State()->GameMemberInfoArray.MarkArrayDirty();
		}

		State()->OnRep_PlayersLeft();
		if (!kPlotMode && !ps->bIsABot)
			Wire::QueryProfile(Wire::AccountId(ps));

		if (kPlotMode && !ps->bIsSpectator)
			Island::ClaimPortal(pc);

		if (auto* list = State() ? State()->CurrentPlaylistInfo.BasePlaylist : nullptr)
			Note("client admitted playlist {}", list->GetName());
		else
			Note("client admitted playlist none");
		if (AdmitOriginal)
			AdmitOriginal(mode, pc);
		Satchel::GiveStarter(pc);
		if (kPlotMode && !pc->Pawn)
		{
			if (auto* gm = Mode())
				gm->RestartPlayer(pc);
		}
		if (auto* pawn = As<AFortPawn>(pc->Pawn))
			Satchel::EquipHarvest(pc, pawn);
		if (kPlotMode)
			Island::LandPlayer(pc);
		Arena::KeepPlaylist();
	}

	inline APawn* SpawnFor(AGameModeBase* mode, AController* controller, AActor* spot)
	{
		if (kPlotMode)
		{
			if (auto* lobby = Island::LobbyStart(controller))
				spot = lobby;
			else if (spot && !Island::IsLobbySpawn(spot))
				spot = nullptr;
		}
		if (!mode || !spot)
			return nullptr;

		auto* pawn = mode->SpawnDefaultPawnAtTransform(controller, spot->GetTransform());
		if (!pawn && World())
		{
			static auto* pawnClass = Find<UBlueprintGeneratedClass>("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
			pawn = World()->SpawnActor<AFortPlayerPawnAthena>(spot->K2_GetActorLocation(), spot->K2_GetActorRotation(), pawnClass);
		}
		if (auto* pc = As<AFortPlayerController>(controller))
		{
			if (auto* athena = As<AFortPlayerPawnAthena>(pawn))
			{
				Paint(pc, athena);
				athena->SetHealth(100);
				athena->SetShield(100);
			}
		}
		return pawn;
	}

	inline bool PossessAck(APlayerController* pc, APawn* pawn)
	{
		if (!pc)
			return true;

		pc->AcknowledgedPawn = pawn;

		auto* fort = As<AFortPlayerController>(pc);
		auto* athena = As<AFortPlayerPawnAthena>(pawn);
		if (fort && athena)
		{
			Paint(fort, athena);
			athena->SetHealth(100);
			athena->SetShield(100);
			if (kStormRush && Satchel::HasLateKit(fort))
				Satchel::EquipLoadout(fort, athena);
			else
				Satchel::EquipHarvest(fort, athena);
		}

		static bool noted = false;
		static int notedRound = 0;
		if (notedRound != Arena::Cycle)
		{
			noted = false;
			notedRound = Arena::Cycle;
		}
		if (!noted)
		{
			noted = true;
			Note("possession accepted");
		}
		return true;
	}

	inline FVector DropSpot()
	{
		FVector loc = Front::Center;
		loc.Z = Front::kBusHeight;
		if (auto* bus = Front::Bus())
		{
			const FVector here = bus->K2_GetActorLocation();
			if (here.Z >= 10000.f)
			{
				loc.X = here.X;
				loc.Y = here.Y;
				loc.Z = here.Z;
			}
		}
		return loc;
	}

	inline AFortPlayerPawnAthena* MakeDropPawn(AFortPlayerController* pc, const FVector& loc, const FRotator& stand)
	{
		if (!pc || !World())
			return nullptr;

		if (auto* old = pc->Pawn)
		{
			pc->UnPossess();
			old->K2_DestroyActor();
		}
		pc->MyFortPawn = nullptr;
		pc->AcknowledgedPawn = nullptr;

		static auto* pawnClass = Find<UBlueprintGeneratedClass>("/Game/Athena/PlayerPawn_Athena.PlayerPawn_Athena_C");
		auto* pawn = World()->SpawnActor<AFortPlayerPawnAthena>(loc, stand, pawnClass);
		if (!pawn)
		{
			if (auto* mode = Mode())
			{
				mode->RestartPlayer(pc);
				pawn = As<AFortPlayerPawnAthena>(pc->Pawn);
				if (pawn)
				{
					FHitResult hit{};
					pawn->K2_SetActorLocationAndRotation(loc, stand, false, true, &hit);
				}
			}
		}
		return pawn;
	}

	inline void DropFromBus(AFortPlayerController* pc, FRotator look, bool force = false)
	{
		if (!pc || Front::HasJumped(pc))
			return;
		if (!force && !pc->IsInAircraft() && !ViewingBus(As<AFortPlayerControllerAthena>(pc), Front::Bus()))
			return;

		if (kStormRush)
			Satchel::LateKit(pc);

		const FVector loc = DropSpot();
		const FRotator stand{ 0.f, look.Yaw, 0.f };

		pc->bAutoManageActiveCameraTarget = true;
		pc->ResetIgnoreMoveInput();
		pc->ResetIgnoreLookInput();
		pc->ClientGotoState(MakeName(L"Playing"));

		auto* pawn = MakeDropPawn(pc, loc, stand);
		if (!pawn)
		{
			Note("drop spawn failed at {:.0f} {:.0f} {:.0f}", loc.X, loc.Y, loc.Z);
			return;
		}

		pc->Possess(pawn);
		pawn->SetActorHiddenInGame(false);
		pawn->SetActorEnableCollision(true);
		pc->MyFortPawn = pawn;
		pc->AcknowledgedPawn = pawn;
		Front::MarkJumped(pc);

		if (auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState))
		{
			if (AircraftOriginal)
				AircraftOriginal(ps, false);
			ps->bInAircraft = 0;
			ps->bIsSpectator = false;
			Specs::SeedPlayer(ps);
		}

		pawn->BeginSkydiving(true);
		Paint(pc, pawn);
		FViewTargetTransitionParams blend{};
		pc->ClientSetViewTarget(pawn, blend);
		pc->ClientSetRotation(look, false);
		Satchel::GiveHarvest(pc);
		Satchel::GiveBuildKit(pc);
		Satchel::Push(pc);
		Satchel::EquipLoadout(pc, pawn);
		pawn->SetHealth(100);
		pawn->SetShield(100);
		Note("dropped from bus");
	}

	inline bool LeftTheBus(AFortPlayerController* pc)
	{
		if (!pc)
			return false;
		auto* pawn = As<AFortPlayerPawnAthena>(pc->MyFortPawn ? pc->MyFortPawn : pc->Pawn);
		if (!pawn)
			return false;
		if (pawn->IsSkydiving() || pawn->IsParachuteOpen() || pawn->IsSkydivingFromBus())
			return true;
		return pawn->K2_GetActorLocation().Z >= 10000.f && !pc->IsInAircraft();
	}

	inline bool JumpBus(AFortPlayerController* pc, FRotator look)
	{
		if (!pc || Front::HasJumped(pc))
			return true;
		Note("aircraft jump");
		DropFromBus(pc, look, true);
		return true;
	}

	inline void ScreenDropped(AFortPlayerControllerAthena* pc)
	{
		if (!kPlotMode && ScreenOriginal)
			ScreenOriginal(pc);
		if (!pc)
			return;
		if (kPlotMode)
		{
			pc->bHasServerFinishedLoading = true;
			pc->OnRep_bHasServerFinishedLoading();
			Satchel::GiveStarter(pc);
			Island::LandPlayer(pc);
			if (auto* pawn = As<AFortPawn>(pc->Pawn))
				Satchel::EquipHarvest(pc, pawn);
			return;
		}
		if (kStormRush && (pc->IsInAircraft() || ViewingBus(pc, Front::Bus())))
			return;
		Satchel::GiveStarter(pc);
		if (auto* pawn = As<AFortPawn>(pc->Pawn))
		{
			if (kStormRush && Satchel::HasLateKit(pc))
				Satchel::EquipLoadout(pc, pawn);
			else
				Satchel::EquipHarvest(pc, pawn);
		}
	}

	inline bool InAircraft(AFortPlayerStateAthena* ps, bool flying)
	{
		if (kPlotMode)
		{
			if (AircraftOriginal)
				AircraftOriginal(ps, false);
			if (ps)
			{
				ps->bInAircraft = 0;
				ps->bIsSpectator = false;
			}
			if (auto* pc = As<AFortPlayerControllerAthena>(ps->GetOwner()))
				Island::LandPlayer(pc);
			return true;
		}
		if (auto* pc = As<AFortPlayerControllerAthena>(ps->GetOwner()))
		{
			if (kStormRush ? flying : true)
			{
				Satchel::EmptyDroppables(pc);
				Satchel::GiveHarvest(pc);
				Satchel::GiveBuildKit(pc);
				Satchel::Push(pc);
			}
		}
		if (AircraftOriginal)
			AircraftOriginal(ps, flying);
		return true;
	}

	inline void EachClient(const std::function<void(AFortPlayerControllerAthena*)>& fn)
	{
		if (auto* mode = Mode())
		{
			for (int i = 0; i < mode->AlivePlayers.Num(); ++i)
			{
				if (mode->AlivePlayers[i])
					fn(mode->AlivePlayers[i]);
			}
		}

		auto* world = World();
		if (!world || !world->NetDriver)
			return;
		for (int i = 0; i < world->NetDriver->ClientConnections.Num(); ++i)
		{
			auto* conn = world->NetDriver->ClientConnections[i];
			auto* pc = conn ? As<AFortPlayerControllerAthena>(conn->PlayerController) : nullptr;
			if (pc)
				fn(pc);
		}
	}

	inline bool ViewingBus(AFortPlayerControllerAthena* pc, AFortAthenaAircraft* bus)
	{
		if (!pc)
			return false;

		auto isBus = [](AActor* actor) -> bool
		{
			if (!actor)
				return false;
			return actor->IsA(AFortAthenaAircraft::StaticClass()) || actor->IsA(AFortAircraft::StaticClass());
		};

		if (auto* view = pc->GetViewTarget())
		{
			if (bus && view == bus)
				return true;
			if (isBus(view))
				return true;
		}
		return false;
	}

	inline bool SeatedInBus(AFortPlayerControllerAthena* pc, AFortAthenaAircraft* bus)
	{
		if (!pc)
			return false;
		if (pc->IsInAircraft())
			return true;
		return ViewingBus(pc, bus);
	}

	inline bool AnyoneInBus()
	{
		auto* bus = Front::Bus();
		bool seated = false;
		EachClient([&](AFortPlayerControllerAthena* pc)
		{
			if (pc && pc->PlayerState && !pc->PlayerState->bIsABot && SeatedInBus(pc, bus))
				seated = true;
		});
		return seated;
	}

	inline void FlushBus()
	{
		EachClient([&](AFortPlayerControllerAthena* pc)
		{
			if (!pc || (pc->PlayerState && pc->PlayerState->bIsABot))
				return;
			if (Front::HasJumped(pc))
				return;
			DropFromBus(pc, pc->GetControlRotation(), true);
		});
	}

	inline void AimCamera(AFortPlayerControllerAthena* pc, AFortAthenaAircraft* bus)
	{
		if (!pc || !bus)
			return;

		pc->bAutoManageActiveCameraTarget = false;
		pc->SetIgnoreMoveInput(true);
		pc->SetIgnoreLookInput(false);
		pc->ClientGotoState(MakeName(L"Spectating"));
		pc->SetViewTargetWithBlend(bus, 0.f, EViewTargetBlendFunction::VTBlend_Linear, 0.f, false);
		FViewTargetTransitionParams blend{};
		pc->ClientSetViewTarget(bus, blend);
		pc->EnterAircraftClient(bus);
	}

	inline void TearDownIslandPawn(AFortPlayerControllerAthena* pc)
	{
		if (!pc)
			return;

		auto* pawn = pc->Pawn;
		if (!pawn)
			return;
		if (pawn->K2_GetActorLocation().Z >= 10000.f)
			return;

		pc->ClientActivateSlot(EFortQuickBars::Primary, 0, 0.f, true, true);
		pawn->SetReplicateMovement(false);
		pawn->SetActorHiddenInGame(true);
		pawn->SetActorEnableCollision(false);
		pc->UnPossess();
		pc->MyFortPawn = nullptr;
		pc->AcknowledgedPawn = nullptr;

		if (auto* reset = Patch::FindReset())
		{
			pawn->K2_DestroyActor();
			reset(pc);
		}
	}

	inline void MarkInAircraft(AFortPlayerControllerAthena* pc)
	{
		auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState);
		if (!ps)
			return;
		if (AircraftOriginal)
			AircraftOriginal(ps, true);
		ps->bInAircraft = 1;
	}

	inline void SeatInBus(AFortPlayerControllerAthena* pc, AFortAthenaAircraft* bus)
	{
		if (!pc || !bus)
			return;
		if (pc->IsInAircraft())
			return;

		static bool seatNote = false;
		static int seatRound = 0;
		if (seatRound != Arena::Cycle)
		{
			seatNote = false;
			seatRound = Arena::Cycle;
		}
		if (!seatNote)
		{
			seatNote = true;
			Note("late bus seat");
		}

		TearDownIslandPawn(pc);
		AimCamera(pc, bus);
		MarkInAircraft(pc);

		Satchel::EmptyDroppables(pc);
		Satchel::GiveHarvest(pc);
		Satchel::GiveBuildKit(pc);
		Satchel::Push(pc);
	}

	inline bool BoardOne(AFortPlayerControllerAthena* pc, AFortAthenaAircraft* bus)
	{
		if (!pc || !bus)
			return false;

		auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState);
		if (!ps || ps->bIsABot)
			return false;

		if (Front::HasJumped(pc))
			return false;

		if (pc->IsInAircraft())
			return true;

		if (ViewingBus(pc, bus))
		{
			MarkInAircraft(pc);
			return true;
		}

		SeatInBus(pc, bus);
		return pc->IsInAircraft() || ViewingBus(pc, bus);
	}

	inline int BoardAlive()
	{
		auto* bus = Front::Bus();
		if (!bus || Front::BusGone)
			return 0;

		int boarded = 0;
		auto* world = World();
		if (world && world->NetDriver && world->NetDriver->ClientConnections.Num())
		{
			for (int i = 0; i < world->NetDriver->ClientConnections.Num(); ++i)
			{
				auto* conn = world->NetDriver->ClientConnections[i];
				auto* pc = conn ? As<AFortPlayerControllerAthena>(conn->PlayerController) : nullptr;
				if (BoardOne(pc, bus))
					++boarded;
			}
		}
		else if (auto* mode = Mode())
		{
			for (int i = 0; i < mode->AlivePlayers.Num(); ++i)
			{
				if (BoardOne(mode->AlivePlayers[i], bus))
					++boarded;
			}
		}

		static bool noted = false;
		static int notedRound = 0;
		if (notedRound != Arena::Cycle)
		{
			noted = false;
			notedRound = Arena::Cycle;
		}
		if (boarded && !noted)
		{
			noted = true;
			Note("players seated in bus ({})", boarded);
		}
		return boarded;
	}

	inline __int64 TeamPick(AFortGameModeAthena* mode, uint8_t, AFortPlayerControllerAthena*)
	{
		auto* list = State() ? State()->CurrentPlaylistInfo.BasePlaylist : nullptr;
		static int next = 3;
		static int packed = 0;
		static int teamRound = 0;
		if (teamRound != Arena::Cycle)
		{
			teamRound = Arena::Cycle;
			next = 3;
			packed = 0;
		}
		if (!list)
			return next++;

		if (packed >= list->MaxSquadSize)
		{
			next++;
			packed = 0;
		}
		packed++;
		return next;
	}
}
