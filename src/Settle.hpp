#pragma once

#include "Pch.hpp"
#include "Arena.hpp"
#include "Bots.hpp"
#include "Satchel.hpp"
#include "Wire.hpp"

namespace Revert::Settle
{
	inline bool Won = false;
	inline bool LoggedStay = false;
	inline std::vector<AFortPlayerControllerAthena*> Placed;

	inline bool IsBot(AFortPlayerController* pc)
	{
		if (!pc || !pc->PlayerState || pc->PlayerState->bIsABot)
			return true;
		return As<AFortAthenaAIBotController>(pc) != nullptr;
	}

	inline bool AlreadyPlaced(AFortPlayerControllerAthena* pc)
	{
		for (auto* it : Placed)
		{
			if (it == pc)
				return true;
		}
		return false;
	}

	inline void MarkPlaced(AFortPlayerControllerAthena* pc)
	{
		if (pc && !AlreadyPlaced(pc))
			Placed.push_back(pc);
	}

	inline int AliveHumans()
	{
		auto* mode = Mode();
		if (!mode)
			return 0;
		int n = 0;
		for (int i = 0; i < mode->AlivePlayers.Num(); ++i)
		{
			auto* pc = mode->AlivePlayers[i];
			if (!pc || IsBot(pc) || AlreadyPlaced(pc))
				continue;
			auto* pawn = As<AFortPawn>(pc->Pawn ? pc->Pawn : pc->MyFortPawn);
			if (pawn && pawn->IsDead())
				continue;
			++n;
		}
		return n;
	}

	inline AFortPlayerControllerAthena* LastHuman()
	{
		auto* mode = Mode();
		if (!mode)
			return nullptr;
		AFortPlayerControllerAthena* hit = nullptr;
		for (int i = 0; i < mode->AlivePlayers.Num(); ++i)
		{
			auto* pc = mode->AlivePlayers[i];
			if (!pc || IsBot(pc) || AlreadyPlaced(pc))
				continue;
			auto* pawn = As<AFortPawn>(pc->Pawn ? pc->Pawn : pc->MyFortPawn);
			if (pawn && pawn->IsDead())
				continue;
			if (hit)
				return nullptr;
			hit = pc;
		}
		return hit;
	}

	inline bool HadOpponents()
	{
		if (Arena::PeakHumans >= 2)
			return true;
		return kSolo && Bots::EverSpawned;
	}

	inline bool CanWin()
	{
		if (Won || kPlotMode)
			return false;
		if (!HadOpponents())
			return false;
		if (AliveHumans() != 1)
			return false;
		if (kSolo && Bots::LivingBots() > 0)
			return false;
		return true;
	}

	inline UFortWeaponItemDefinition* WeaponOf(AActor* causer)
	{
		if (auto* weapon = As<AFortWeapon>(causer))
			return weapon->WeaponData;
		if (!causer)
			return nullptr;
		if (auto* owner = causer->GetOwner())
		{
			if (auto* weapon = As<AFortWeapon>(owner))
				return weapon->WeaponData;
			if (auto* pawn = As<AFortPawn>(owner); pawn && pawn->CurrentWeapon)
				return pawn->CurrentWeapon->WeaponData;
		}
		return nullptr;
	}

	inline void DropBag(AFortPlayerControllerAthena* pc)
	{
		if (!pc || !pc->MyFortPawn || !pc->WorldInventory)
			return;
		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		const auto loc = pc->MyFortPawn->K2_GetActorLocation();
		for (int i = 0; i < bag.Num(); ++i)
		{
			if (bag[i] && bag[i]->CanBeDropped())
				Satchel::Toss(bag[i]->ItemEntry.ItemDefinition, bag[i]->ItemEntry.Count, loc, bag[i]->ItemEntry.LoadedAmmo);
		}
		Satchel::EmptyDroppables(pc);
	}

	inline void PullAlive(AFortPlayerControllerAthena* pc, AFortPlayerStateAthena* killer, AFortPlayerPawnAthena* dead, UFortWeaponItemDefinition* weapon, uint8_t cause)
	{
		auto* mode = Mode();
		if (!mode || !pc)
			return;
		using RemoveFn = void (*)(AFortGameModeAthena*, AFortPlayerControllerAthena*, AFortPlayerStateAthena*, AFortPlayerPawnAthena*, UFortWeaponItemDefinition*, uint8_t, char);
		static auto Remove = Rva::Rel<RemoveFn>(Rva::RemoveAlive);
		Remove(mode, pc, killer, dead, weapon, cause, 0);
	}

	inline void SyncCount()
	{
		auto* state = State();
		if (!state)
			return;
		if (kSolo)
		{
			Bots::SyncPlayersLeft();
			return;
		}
		auto* mode = Mode();
		if (!mode)
			return;
		int n = 0;
		for (int i = 0; i < mode->AlivePlayers.Num(); ++i)
		{
			auto* pc = mode->AlivePlayers[i];
			if (pc && !AlreadyPlaced(pc))
				++n;
		}
		if (state->PlayersLeft != n)
		{
			state->PlayersLeft = n;
			state->OnRep_PlayersLeft();
		}
	}

	inline void DeclareWin(AFortPlayerControllerAthena* pc, AFortPawn* pawn, UFortWeaponItemDefinition* weapon, EDeathCause cause)
	{
		if (!CanWin() || !pc)
			return;
		auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState);
		auto* state = State();
		if (!ps || !state)
			return;
		Won = true;
		ps->Place = 1;
		ps->OnRep_Place();
		ps->bHasWonAGame = true;
		state->WinningPlayerState = ps;
		state->OnRep_WinningPlayerState();
		state->WinningTeam = ps->TeamIndex;
		state->OnRep_WinningTeam();
		const auto old = state->GamePhase;
		state->GamePhase = EAthenaGamePhase::EndGame;
		state->OnRep_GamePhase(old);
		pc->ClientNotifyWon(pawn, weapon, cause);
		pc->ClientNotifyTeamWon(pawn, weapon, cause);
		pc->ClientReportTournamentPlacementPointsScored(1, Cfg::WinHype);
		const auto id = Wire::AccountId(ps);
		Wire::SavePlace(id, 1, ps->KillScore, true);
		Note("winner  {}  kills {}", ps->GetPlayerName().ToString(), ps->KillScore);
		Arena::QueueReload();
	}

	inline void HandleDeath(AFortPlayerControllerAthena* pc, FFortPlayerDeathReport report)
	{
		if (kPlotMode || !pc || Won)
			return;
		auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState);
		if (!ps || IsBot(pc) || AlreadyPlaced(pc))
			return;
		if (pc->MyFortPawn && pc->MyFortPawn->IsDBNO())
			return;

		auto* state = State();
		auto* mode = Mode();
		if (!state || !mode)
			return;
		if (state->GamePhase < EAthenaGamePhase::Aircraft)
			return;

		MarkPlaced(pc);
		DropBag(pc);

		auto* killerPs = As<AFortPlayerStateAthena>(report.KillerPlayerState);
		auto* killerPawn = As<AFortPlayerPawnAthena>(report.KillerPawn);
		auto* deadPawn = As<AFortPlayerPawnAthena>(pc->MyFortPawn ? pc->MyFortPawn : pc->Pawn);
		auto* weapon = WeaponOf(report.DamageCauser);

		ps->DeathInfo.bDBNO = deadPawn ? deadPawn->bWasDBNOOnDeath : false;
		ps->DeathInfo.bInitialized = true;
		ps->DeathInfo.DeathLocation = deadPawn ? deadPawn->K2_GetActorLocation() : FVector{};
		ps->DeathInfo.DeathTags = report.Tags;
		ps->DeathInfo.Downer = killerPs;
		ps->DeathInfo.FinisherOrDowner = killerPs ? static_cast<AActor*>(killerPs) : static_cast<AActor*>(ps);
		ps->DeathInfo.Distance = killerPawn && deadPawn ? killerPawn->GetDistanceTo(deadPawn) : (deadPawn ? deadPawn->LastFallDistance : 0.f);
		ps->DeathInfo.DeathCause = AFortPlayerStateAthena::ToDeathCause(report.Tags, ps->DeathInfo.bDBNO);
		ps->OnRep_DeathInfo();

		const int left = state->PlayersLeft > 0 ? state->PlayersLeft : AliveHumans() + 1;
		ps->Place = left;
		ps->OnRep_Place();
		pc->ClientReportTournamentPlacementPointsScored(ps->Place, Wire::PlaceHype(ps->Place));
		pc->ClientNotifyLost(killerPawn, EEndOfMatchReason::LastManStanding);

		if (killerPs && killerPs != ps)
		{
			killerPs->KillScore++;
			killerPs->TeamKillScore++;
			killerPs->OnRep_Kills();
			killerPs->OnRep_TeamKillScore();
			killerPs->ClientReportKill(ps);
			if (killerPawn && killerPawn != deadPawn && (Cfg::SiphonHealth > 0.f || Cfg::SiphonShield > 0.f))
			{
				if (Cfg::SiphonHealth > 0.f)
					killerPawn->SetHealth(Cfg::SiphonHealth);
				if (Cfg::SiphonShield > 0.f)
					killerPawn->SetShield(Cfg::SiphonShield);
			}
			if (!killerPs->bIsABot)
				Wire::SaveKill(Wire::AccountId(killerPs));
		}

		PullAlive(pc, killerPs && killerPs != ps ? killerPs : nullptr, deadPawn, weapon, static_cast<uint8_t>(ps->DeathInfo.DeathCause));
		SyncCount();

		const auto id = Wire::AccountId(ps);
		Wire::SavePlace(id, ps->Place, ps->KillScore, false);
		Note("eliminated  {}  place {}", ps->GetPlayerName().ToString(), ps->Place);

		if (auto* winner = LastHuman(); winner && CanWin())
		{
			auto* winPawn = As<AFortPawn>(winner->Pawn ? winner->Pawn : winner->MyFortPawn);
			auto* winPs = As<AFortPlayerStateAthena>(winner->PlayerState);
			DeclareWin(winner, winPawn, weapon, winPs ? winPs->DeathInfo.DeathCause : ps->DeathInfo.DeathCause);
		}
		else if (AliveHumans() <= 0)
		{
			const auto old = state->GamePhase;
			state->GamePhase = EAthenaGamePhase::EndGame;
			state->OnRep_GamePhase(old);
			Arena::QueueReload();
		}
	}

	inline void Reset()
	{
		Won = false;
		LoggedStay = false;
		Placed.clear();
	}

	inline void Pulse()
	{
		static UWorld* last = nullptr;
		auto* world = World();
		if (world != last)
		{
			last = world;
			Reset();
		}
		if (kPlotMode || Won)
			return;
		auto* state = State();
		if (!state || state->GamePhase < EAthenaGamePhase::SafeZones)
			return;
		if (auto* winner = LastHuman())
		{
			if (CanWin())
			{
				auto* pawn = As<AFortPawn>(winner->Pawn ? winner->Pawn : winner->MyFortPawn);
				DeclareWin(winner, pawn, nullptr, EDeathCause::Unspecified);
			}
			else if (!HadOpponents() && AliveHumans() == 1 && !LoggedStay)
			{
				LoggedStay = true;
				Note("one player, no opponents — staying in match");
			}
		}
	}
}
