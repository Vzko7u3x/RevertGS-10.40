#pragma once

#include "Pch.hpp"
#include "Patch.hpp"
#include "Front.hpp"
#include "Island.hpp"
#include "Satchel.hpp"

namespace Revert::Arena
{
	inline bool (*ReadyOriginal)(AFortGameModeAthena*) = nullptr;
	inline bool Listening = false;
	inline int Cycle = 1;
	inline bool* FirstJoinPtr = nullptr;
	inline void (*OnRoundReset)() = nullptr;
	inline bool ReloadQueued = false;
	inline bool Reloading = false;
	inline bool ReloadForced = false;
	inline bool HadHumans = false;
	inline bool ReloadWaitNote = false;
	inline bool ReloadQuietNote = false;
	inline bool SettleNote = false;
	inline bool SendingHome = false;
	inline bool KillQueued = false;
	inline bool Settling = false;
	inline int PeakHumans = 0;
	inline float ReloadAt = 0.f;
	inline float ReloadQueuedAt = 0.f;
	inline float ReloadQuietUntil = 0.f;
	inline float LastLogoutAt = 0.f;
	inline float SettleUntil = 0.f;
	inline bool LootHooked = false;
	inline constexpr float kReloadQuiet = 20.f;
	inline constexpr float kReloadSettle = 20.f;
	inline constexpr float kOssLeaveWait = 25.f;
	inline constexpr float kReloadSlotWait = 15.f;

	inline void HoldCreative();
	inline void ParkBus();
	inline void LaunchBus();
	inline void SendHome(APlayerController* pc);

	inline const char* WantedPlaylistLeaf()
	{
		if (kPlotMode)
			return "Playlist_PlaygroundV2";
		if (kStormRush)
			return "Playlist_ShowdownAlt_Solo";
		return "Playlist_DefaultSolo";
	}

	inline bool PlaylistMatches(UFortPlaylistAthena* list)
	{
		if (!list)
			return false;
		if (auto* type = UFortPlaylistAthena::StaticClass(); type && !list->IsA(type))
			return false;
		return list->GetName().find(WantedPlaylistLeaf()) != std::string::npos;
	}

	inline UFortPlaylistAthena* ScanNamedPlaylist(const char* leaf)
	{
		if (!UObject::GObjects || !leaf || !*leaf)
			return nullptr;

		auto* type = UFortPlaylistAthena::StaticClass();
		UFortPlaylistAthena* fuzzy = nullptr;
		for (int i = 0; i < UObject::GObjects->Num(); ++i)
		{
			auto* object = UObject::GObjects->GetObjectById(i);
			if (!object)
				continue;
			if (type && !object->IsA(type))
				continue;
			if (!type && object->Class && object->Class->GetName().find("FortPlaylistAthena") == std::string::npos)
				continue;

			const auto name = object->GetName();
			if (name.find("Default__") != std::string::npos)
				continue;
			if (name == leaf)
				return static_cast<UFortPlaylistAthena*>(object);
			if (!fuzzy && name.find(leaf) != std::string::npos)
				fuzzy = static_cast<UFortPlaylistAthena*>(object);
		}
		return fuzzy;
	}

	inline UFortPlaylistAthena* Playlist()
	{
		const char* path = kPlotMode
			? "/Game/Athena/Playlists/Creative/Playlist_PlaygroundV2.Playlist_PlaygroundV2"
			: kStormRush
				? "/Game/Athena/Playlists/Showdown/Playlist_ShowdownAlt_Solo.Playlist_ShowdownAlt_Solo"
				: "/Game/Athena/Playlists/Playlist_DefaultSolo.Playlist_DefaultSolo";
		const char* leaf = WantedPlaylistLeaf();

		if (auto* list = Find<UFortPlaylistAthena>(path); PlaylistMatches(list))
			return list;

		if (auto* list = UObject::FindObjectSlow<UFortPlaylistAthena>(leaf); PlaylistMatches(list))
			return list;

		if (kStormRush)
		{
			if (auto* list = ScanNamedPlaylist("ShowdownAlt"))
				return list;
		}

		return ScanNamedPlaylist(leaf);
	}

	inline void ApplyPlaylist(UFortPlaylistAthena* list)
	{
		auto* state = State();
		auto* mode = Mode();
		if (!state || !mode)
			return;

		if (!list)
		{
			static bool missing = false;
			if (!missing)
			{
				missing = true;
				Note("playlist missing");
			}
			return;
		}

		const bool changed = state->CurrentPlaylistInfo.BasePlaylist != list;
		state->CurrentPlaylistInfo.BasePlaylist = list;
		state->CurrentPlaylistInfo.OverridePlaylist = list;
		if (changed)
			state->CurrentPlaylistInfo.PlaylistReplicationKey++;
		state->CurrentPlaylistInfo.MarkArrayDirty();
		mode->CurrentPlaylistId = list->PlaylistId;
		mode->CurrentPlaylistName = list->PlaylistName;
		state->CurrentPlaylistId = list->PlaylistId;
		state->OnRep_CurrentPlaylistInfo();
		state->OnRep_CurrentPlaylistId();
		if (kStormRush)
			Front::TuneList(list);
		if (kPlotMode)
		{
			list->bSkipWarmup = false;
			list->bSkipAircraft = true;
			list->AirCraftBehavior = EAirCraftBehavior::NoAircraft;
			list->bEnableCreativeMode = 1;
			state->bGameModeWillSkipAircraft = true;
			state->AirCraftBehavior = EAirCraftBehavior::NoAircraft;
		}
		if (changed)
			Note("playlist {}", list->GetName());
	}

	inline void KeepPlaylist()
	{
		auto* state = State();
		auto* current = state ? state->CurrentPlaylistInfo.BasePlaylist : nullptr;
		if (PlaylistMatches(current))
			return;
		ApplyPlaylist(Playlist());
	}

	inline void ExtraLevels(UFortPlaylist* list)
	{
		if (!list)
			return;
		for (int i = 0; i < list->AdditionalLevels.Num(); ++i)
		{
			const auto path = list->AdditionalLevels[i].ObjectID.AssetPathName.ToString();
			if (path.empty())
				continue;
			std::wstring wide(path.begin(), path.end());
			bool ok = false;
			ULevelStreamingDynamic::LoadLevelInstance(World(), wide.c_str(), FVector{}, FRotator{}, &ok);
		}
	}

	inline void OpenListen()
	{
		auto* world = World();
		auto* mode = Mode();
		auto* state = State();
		if (!world || !mode || Listening)
			return;

		using CreateFn = UNetDriver* (*)(UEngine*, UWorld*, FName);
		using ListenFn = char (*)(UNetDriver*, void*, FURL&, bool, FString&);
		using BindFn = void (*)(UNetDriver*, UWorld*);

		auto Create = Rva::Rel<CreateFn>(Rva::CreateNetDriver);
		auto Listen = Rva::Rel<ListenFn>(Rva::InitListen);
		auto Bind = Rva::Rel<BindFn>(Rva::SetWorld);

		Note("round {} — creating net driver on {}", Cycle, Cfg::ListenPort);
		world->NetDriver = Create(Engine(), world, MakeName(L"GameNetDriver"));
		if (!world->NetDriver)
		{
			Note("round {} — listen driver missing", Cycle);
			return;
		}

		world->NetDriver->World = world;
		world->NetDriver->NetDriverName = MakeName(L"GameNetDriver");

		FURL url{};
		url.Port = Cfg::ListenPort;
		FString error;
		const char ok = Listen(world->NetDriver, world, url, true, error);
		if (!ok)
		{
			const auto why = error.ToString();
			Note("round {} — InitListen failed  {}", Cycle, why.empty() ? "unknown" : why);
			world->NetDriver = nullptr;
			return;
		}
		Bind(world->NetDriver, world);

		if (world->LevelCollections.Num() > 0)
			world->LevelCollections[0].NetDriver = world->NetDriver;
		if (world->LevelCollections.Num() > 1)
			world->LevelCollections[1].NetDriver = world->NetDriver;

		if (mode->GameSession)
			mode->GameSession->MaxPlayers = Cfg::MaxPlayers;
		mode->bWorldIsReady = true;
		mode->WarmupRequiredPlayerCount = Cfg::WarmupRequired;
		state->PlayersLeft--;
		state->OnRep_PlayersLeft();
		state->DefaultRebootMachineHotfix = 1;
		Listening = true;
		Note("listening on {} ({})  round {}", Cfg::ListenPort, kFlavorName, Cycle);
	}

	inline int Humans()
	{
		auto* world = World();
		if (!world || !world->NetDriver)
			return 0;
		int n = 0;
		for (int i = 0; i < world->NetDriver->ClientConnections.Num(); ++i)
		{
			auto* conn = world->NetDriver->ClientConnections[i];
			auto* pc = conn ? As<AFortPlayerControllerAthena>(conn->PlayerController) : nullptr;
			if (pc && pc->PlayerState && !pc->PlayerState->bIsABot)
				++n;
		}
		return n;
	}

	inline int ClientSlots()
	{
		auto* world = World();
		if (!world || !world->NetDriver)
			return 0;
		return world->NetDriver->ClientConnections.Num();
	}

	inline bool CanLaunchBus()
	{
		return !ReloadQueued && !Reloading && !Settling;
	}

	inline constexpr wchar_t kHomeReason[] = L"None";

	inline bool HomeReasonEmpty(const FString& reason)
	{
		if (!reason.Data || reason.Count <= 0)
			return true;
		return reason.Data[0] == 0;
	}

	inline FString CopyHomeReason()
	{
		const int n = static_cast<int>(sizeof(kHomeReason) / sizeof(kHomeReason[0]));
		FString out;
		out.Data = static_cast<wchar_t*>(FMemory_Realloc(nullptr, static_cast<__int64>(n) * sizeof(wchar_t), 0));
		if (!out.Data)
			return FString(kHomeReason);
		std::memcpy(out.Data, kHomeReason, sizeof(kHomeReason));
		out.Count = n;
		out.Max = n;
		return out;
	}

	inline void FillHomeReason(FString& reason)
	{
		if (HomeReasonEmpty(reason))
			reason = CopyHomeReason();
	}

	inline bool OssLeavePending(float now)
	{
		if (LastLogoutAt <= 0.f)
			return false;
		return now < LastLogoutAt + kOssLeaveWait;
	}

	inline float OssLeaveUntil(float now)
	{
		if (LastLogoutAt > 0.f)
			return LastLogoutAt + kOssLeaveWait;
		return now + kReloadSettle + kOssLeaveWait;
	}

	inline void StretchQuiet(float now)
	{
		const float until = now + kReloadQuiet;
		if (until > ReloadQuietUntil)
			ReloadQuietUntil = until;
		if (Settling)
		{
			float settle = now + kReloadSettle;
			const float oss = OssLeaveUntil(now);
			if (oss > settle)
				settle = oss;
			if (settle > SettleUntil)
				SettleUntil = settle;
		}
	}

	inline void NoteLogout(AController* who)
	{
		(void)who;
		const float now = World() ? UGameplayStatics::GetTimeSeconds(World()) : 0.f;
		LastLogoutAt = now;
		StretchQuiet(now);
	}

	inline void SendHome(APlayerController* pc)
	{
		if (!pc || SendingHome)
			return;
		SendingHome = true;
		pc->ClientReturnToMainMenu(CopyHomeReason());
		SendingHome = false;
		Note("send home");
	}

	inline void DismissClients()
	{
		auto* world = World();
		if (!world || !world->NetDriver)
			return;
		int n = 0;
		for (int i = 0; i < world->NetDriver->ClientConnections.Num(); ++i)
		{
			auto* conn = world->NetDriver->ClientConnections[i];
			auto* pc = conn ? As<APlayerController>(conn->PlayerController) : nullptr;
			if (!pc)
				continue;
			SendHome(pc);
			++n;
		}
		if (n)
			Note("sent {} client(s) to lobby", n);
	}

	inline void CloseListen()
	{
		auto* world = World();
		if (world && world->NetDriver && Engine())
		{
			using ShutFn = void (*)(UEngine*, UWorld*);
			auto Shut = Rva::Rel<ShutFn>(Rva::ShutdownNet);
			Shut(Engine(), world);
			world->NetDriver = nullptr;
			if (world->LevelCollections.Num() > 0)
				world->LevelCollections[0].NetDriver = nullptr;
			if (world->LevelCollections.Num() > 1)
				world->LevelCollections[1].NetDriver = nullptr;
		}
		Listening = false;
		Note("round {} — listen closed", Cycle);
	}

	inline void KillGs()
	{
		Note("match over — killing gs");
		std::cout.flush();
		TerminateProcess(GetCurrentProcess(), 1);
	}

	inline void QueueReload(bool force = false)
	{
		if (ReloadQueued || Reloading)
			return;
		if (!Listening && !force)
			return;
		ReloadQueued = true;
		ReloadForced = force;
		ReloadWaitNote = false;
		ReloadQuietNote = false;
		SettleNote = false;
		Settling = false;
		ReloadQuietUntil = 0.f;
		SettleUntil = 0.f;
		Front::HoldPulse = true;
		const float now = World() ? UGameplayStatics::GetTimeSeconds(World()) : 0.f;
		ReloadQueuedAt = now;
		ReloadAt = now + Cfg::CloseGsDelay;
		DismissClients();
		if (kPlotMode)
		{
			Note("match over — next round after client leaves");
			return;
		}
		KillQueued = true;
		Note("match over — gs will exit");
	}

	inline bool MatchIsOver()
	{
		if (kPlotMode)
			return false;
		auto* state = State();
		auto* mode = Mode();
		if (state && state->GamePhase >= EAthenaGamePhase::EndGame)
			return true;
		if (mode && mode->MatchState.ToString() == "WaitingPostMatch")
			return true;
		if (state && state->MatchState.ToString() == "WaitingPostMatch")
			return true;
		return false;
	}

	inline void RestoreMatch()
	{
		MarkDedicated();
		KeepPlaylist();

		auto* mode = Mode();
		auto* state = State();
		if (!mode || !state)
			return;

		const auto inProgress = MakeName(L"InProgress");
		mode->MatchState = inProgress;
		mode->K2_OnSetMatchState(inProgress);
		state->PreviousMatchState = state->MatchState;
		state->MatchState = inProgress;
		state->OnRep_MatchState();

		state->WinningPlayerState = nullptr;
		state->WinningTeam = 0;
		state->WinningScore = 0;
		state->OnRep_WinningPlayerState();
		state->OnRep_WinningTeam();
		state->OnRep_WinningScore();

		mode->bWorldIsReady = true;
		mode->WarmupRequiredPlayerCount = Cfg::WarmupRequired;
		if (mode->GameSession)
			mode->GameSession->MaxPlayers = Cfg::MaxPlayers;

		if (state->GamePhase >= EAthenaGamePhase::EndGame || state->GamePhase < EAthenaGamePhase::Warmup)
		{
			const auto old = state->GamePhase;
			state->GamePhase = EAthenaGamePhase::Warmup;
			state->GamePhaseStep = EAthenaGamePhaseStep::Warmup;
			state->OnRep_GamePhase(old);
		}

		if (kPlotMode)
		{
			HoldCreative();
			return;
		}

		const float now = UGameplayStatics::GetTimeSeconds(World());
		const float hold = 99999.f;
		state->WarmupCountdownEndTime = now + hold;
		mode->WarmupCountdownDuration = hold;
		state->WarmupCountdownStartTime = now;
		mode->WarmupEarlyCountdownDuration = hold;
	}

	inline void ReloadMatch()
	{
		if (Reloading || !World())
			return;

		Reloading = true;
		ReloadQueued = false;
		ReloadForced = false;
		ReloadWaitNote = false;
		ReloadQuietNote = false;
		SettleNote = false;
		ReloadQuietUntil = 0.f;
		++Cycle;
		PeakHumans = 0;
		HadHumans = false;
		if (FirstJoinPtr)
			*FirstJoinPtr = true;
		if (OnRoundReset)
			OnRoundReset();
		Front::ResetMatch();
		RestoreMatch();
		if (kStormRush)
			ParkBus();

		Reloading = false;
		Front::HoldPulse = true;
		if (!Listening)
			OpenListen();
		const float now = World() ? UGameplayStatics::GetTimeSeconds(World()) : 0.f;
		Settling = true;
		SettleUntil = now + kReloadSettle;
		const float ossUntil = OssLeaveUntil(now);
		if (ossUntil > SettleUntil)
			SettleUntil = ossUntil;
		Note("round {} — restored, settling {:.0f}s before joins", Cycle, SettleUntil - now);
		std::cout.flush();
	}

	inline void WatchPlayers()
	{
		if (!Listening || Reloading || ReloadQueued || Settling)
			return;
		const int n = Humans();
		if (n > PeakHumans)
			PeakHumans = n;
		if (n > 0)
			HadHumans = true;
		else if (HadHumans)
			QueueReload();
	}

	inline void WatchMatchEnd()
	{
		if (kPlotMode || !Listening || Reloading || ReloadQueued || Settling)
			return;
		if (MatchIsOver() && HadHumans)
			QueueReload();
	}

	inline void FinishSettle()
	{
		const float now = World() ? UGameplayStatics::GetTimeSeconds(World()) : 0.f;
		if (OssLeavePending(now))
			return;
		Settling = false;
		SettleNote = false;
		Front::HoldPulse = false;
		if (FirstJoinPtr && *FirstJoinPtr && Humans() > 0)
		{
			*FirstJoinPtr = false;
			LaunchBus();
		}
		Note("round {} — warmup ready  listening on {}", Cycle, Cfg::ListenPort);
		std::cout.flush();
	}

	inline void TickReload()
	{
		WatchMatchEnd();
		WatchPlayers();
		if (KillQueued)
			return;
		const float now = World() ? UGameplayStatics::GetTimeSeconds(World()) : 0.f;

		if (Settling && !ReloadQueued && !Reloading)
		{
			if (now < SettleUntil || OssLeavePending(now))
			{
				if (!SettleNote)
				{
					SettleNote = true;
					Note("round {} — waiting for session teardown", Cycle);
				}
				return;
			}
			FinishSettle();
			return;
		}

		if (!ReloadQueued || Reloading)
			return;
		if (now < ReloadAt)
			return;

		const int slots = ClientSlots();
		if (Humans() > 0)
		{
			Note("client already here — starting round {}", Cycle + 1);
			ReloadMatch();
			return;
		}
		if (slots > 0)
		{
			ReloadQuietUntil = 0.f;
			ReloadQuietNote = false;
			if (now < ReloadQueuedAt + kReloadSlotWait)
			{
				if (!ReloadWaitNote)
				{
					ReloadWaitNote = true;
					Note("waiting for client disconnect before round {} ({} slot)", Cycle + 1, slots);
					DismissClients();
				}
				return;
			}
			Note("forcing round {} with {} leftover slot(s)", Cycle + 1, slots);
			ReloadMatch();
			return;
		}

		if (ReloadQuietUntil <= 0.f)
		{
			const float fromLogout = LastLogoutAt > ReloadQueuedAt - 1.f
				? LastLogoutAt + kReloadQuiet
				: now + kReloadQuiet;
			ReloadQuietUntil = fromLogout > now ? fromLogout : now + kReloadQuiet;
		}
		if (now < ReloadQuietUntil)
		{
			if (!ReloadQuietNote)
			{
				ReloadQuietNote = true;
				Note("client gone — waiting until logout settles before round {}", Cycle + 1);
			}
			return;
		}

		ReloadMatch();
	}

	inline int CountStarts()
	{
		auto count = [](UClass* type) -> int
		{
			if (!type)
				return 0;
			TArray<AActor*> found;
			UGameplayStatics::GetAllActorsOfClass(World(), type, &found);
			const int n = found.Num();
			found.Free();
			return n;
		};

		if (!kPlotMode)
			return count(AFortPlayerStartWarmup::StaticClass());
		if (const int n = count(AFortPlayerStartCreative::StaticClass()))
			return n;
		if (const int n = count(AFortPlayerStart::StaticClass()))
			return n;
		return count(APlayerStart::StaticClass());
	}

	inline void HoldCreative()
	{
		auto* mode = Mode();
		auto* state = State();
		if (!mode || !state)
			return;

		state->bGameModeWillSkipAircraft = true;
		state->AirCraftBehavior = EAirCraftBehavior::NoAircraft;
		if (auto* list = state->CurrentPlaylistInfo.BasePlaylist)
		{
			list->bSkipWarmup = false;
			list->bSkipAircraft = true;
			list->AirCraftBehavior = EAirCraftBehavior::NoAircraft;
			list->bEnableCreativeMode = 1;
		}

		if (state->GamePhase != EAthenaGamePhase::Warmup)
		{
			const auto old = state->GamePhase;
			state->GamePhase = EAthenaGamePhase::Warmup;
			state->GamePhaseStep = EAthenaGamePhaseStep::Warmup;
			state->OnRep_GamePhase(old);
		}

		const float now = UGameplayStatics::GetTimeSeconds(World());
		const float hold = 99999.f;
		state->WarmupCountdownEndTime = now + hold;
		mode->WarmupCountdownDuration = hold;
		state->WarmupCountdownStartTime = now;
		mode->WarmupEarlyCountdownDuration = hold;
		mode->WarmupRequiredPlayerCount = 1;
		Note("creative hold warmup — no bus");
	}

	inline void ParkBus()
	{
		Front::Layout();
	}

	inline void RollContainers()
	{
		auto* state = State();
		if (!state || !state->MapInfo)
			return;

		auto cull = [](UClass* type, int keepMin, int keepMax)
		{
			TArray<AActor*> all;
			UGameplayStatics::GetAllActorsOfClass(World(), type, &all);
			if (!all.Num())
				return;
			const int keep = (all.Num() * RandRange(keepMin, keepMax)) / 100;
			int drop = all.Num() - keep;
			while (drop-- > 0)
				all[std::rand() % all.Num()]->K2_DestroyActor();
			all.Free();
		};

		if (state->MapInfo->TreasureChestClass)
			cull(state->MapInfo->TreasureChestClass, 65, 70);
		if (state->MapInfo->AmmoBoxClass)
			cull(state->MapInfo->AmmoBoxClass, 65, 80);
	}

	inline void SeedVehicles()
	{
		TArray<AActor*> spawners;
		UGameplayStatics::GetAllActorsOfClass(World(), AFortAthenaVehicleSpawner::StaticClass(), &spawners);
		for (int i = 0; i < spawners.Num(); ++i)
		{
			if (auto* spawner = As<AFortAthenaVehicleSpawner>(spawners[i]))
				World()->SpawnActor<AFortAthenaVehicle>(spawner->K2_GetActorLocation(), spawner->K2_GetActorRotation(), spawner->GetVehicleClass());
		}
		spawners.Free();
	}

	inline void ScatterFloor()
	{
		auto drop = [](UClass* type)
		{
			TArray<AActor*> spots;
			UGameplayStatics::GetAllActorsOfClass(World(), type, &spots);
			for (int i = 0; i < spots.Num(); ++i)
			{
				auto* box = As<ABuildingContainer>(spots[i]);
				if (!box)
					continue;
				box->bAlreadySearched = true;
				box->OnRep_bAlreadySearched();
			}
			spots.Free();
		};

		drop(Find<UBlueprintGeneratedClass>("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_Warmup.Tiered_Athena_FloorLoot_Warmup_C"));
		drop(Find<UBlueprintGeneratedClass>("/Game/Athena/Environments/Blueprints/Tiered_Athena_FloorLoot_01.Tiered_Athena_FloorLoot_01_C"));
	}

	inline bool OnReady(AFortGameModeAthena* mode)
	{
		MarkDedicated();
		if (!Reloading)
			Front::HoldPulse = false;

		if (auto* gi = World()->OwningGameInstance; gi && gi->LocalPlayers.Num())
			gi->LocalPlayers.Remove(0);

		if (!CountStarts())
		{
			if (Reloading)
			{
				static int startWaits = 0;
				if ((++startWaits % 60) == 1)
					Note("round {} — waiting for warmup starts", Cycle);
			}
			return false;
		}

		auto* state = State();
		if (!kPlotMode && !state->MapInfo)
		{
			if (Reloading)
			{
				static int mapWaits = 0;
				if ((++mapWaits % 60) == 1)
					Note("round {} — waiting for MapInfo", Cycle);
			}
			return false;
		}
		KeepPlaylist();
		if (!PlaylistMatches(state->CurrentPlaylistInfo.BasePlaylist))
		{
			static int waits = 0;
			if ((++waits % 60) == 1)
				Note("waiting for {}", WantedPlaylistLeaf());
			return false;
		}

		if (!Listening)
		{
			static UWorld* setupWorld = nullptr;
			auto* world = World();
			if (setupWorld != world)
			{
				setupWorld = world;
				ReloadQueued = false;
				ReloadForced = false;
				HadHumans = false;
				PeakHumans = 0;
				Front::ResetMatch();
				if (FirstJoinPtr)
					*FirstJoinPtr = true;
				if (OnRoundReset)
					OnRoundReset();
				Island::WakeFoundation(Find<ABuildingFoundation>("/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.SLAB_4"));
				Island::WakeFoundation(Find<ABuildingFoundation>("/Game/Athena/Maps/Athena_POI_Foundations.Athena_POI_Foundations.PersistentLevel.LF_Athena_StreamingTest16"));
				ExtraLevels(Playlist());
			}
			OpenListen();
			if (!Listening)
			{
				static int listenWaits = 0;
				if ((++listenWaits % 60) == 1)
					Note("round {} — listen not up, will retry", Cycle);
				return false;
			}
			Reloading = false;
			ReloadQueued = false;
			setupWorld = nullptr;
			Front::HoldPulse = false;
			if (kPlotMode)
				HoldCreative();
			else
			{
				RollContainers();
				SeedVehicles();
				ParkBus();
			}

			if (!LootHooked)
			{
				static char (*LootNative)(ABuildingContainer*, AFortPlayerPawnAthena*, int, int) = Rva::Rel<decltype(LootNative)>(Rva::SpawnLoot);
				auto loot = +[](ABuildingContainer* box, AFortPlayerPawnAthena*, int, int) -> char
				{
					if (!box)
						return 0;
					box->bAlreadySearched = true;
					box->SearchBounceData.SearchAnimationCount++;
					box->OnRep_bAlreadySearched();
					const auto spot = box->K2_GetActorLocation() + box->GetActorRightVector() * 70.f + FVector{ 0, 0, 50 };

					static UFortItemDefinition* weaponPool[] = {
						Satchel::FindDef("WID_Assault_Auto_Athena_R_Ore_T03"),
						Satchel::FindDef("WID_Assault_AutoHigh_Athena_SR_Ore_T03"),
						Satchel::FindDef("WID_Shotgun_Standard_Athena_SR_Ore_T03"),
						Satchel::FindDef("WID_Shotgun_HighSemiAuto_Athena_VR_Ore_T03"),
						Satchel::FindDef("WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03"),
						Satchel::FindDef("WID_Sniper_Heavy_Athena_VR_Ore_T03"),
						Satchel::FindDef("WID_Pistol_Scavenger_Athena_VR_Ore_T03"),
						Satchel::FindDef("WID_Pistol_AutoHeavyPDW_Athena_R_Ore_T03"),
						Satchel::FindDef("WID_Pistol_AutoHeavyPDW_Athena_VR_Ore_T03"),
						Satchel::FindDef("WID_Launcher_Rocket_Athena_SR_Ore_T03"),
						Satchel::FindDef("WID_Launcher_Grenade_Athena_R_Ore_T03"),
					};

					int weaponCount = 0;
					for (auto* w : weaponPool)
						if (w) ++weaponCount;

					if (weaponCount > 0)
					{
						int pick = RandRange(0, weaponCount - 1);
						for (auto* w : weaponPool)
						{
							if (!w)
								continue;
							if (pick-- == 0)
							{
								Satchel::TossFromContainer(w, 1, spot, Satchel::ClipOf(w));
								break;
							}
						}
					}

					static UFortItemDefinition* bonusPool[] = {
						Satchel::FindDef("Athena_ShieldSmall"),
						Satchel::FindDef("Athena_Shields"),
						Satchel::FindDef("Athena_Medkit"),
						Satchel::FindDef("Athena_PurpleStuff"),
						Satchel::FindDef("WID_Athena_Flopper"),
						Satchel::FindDef("Athena_GrapplingHook"),
						Satchel::FindDef("Athena_ShockGrenade"),
						Satchel::FindDef("Athena_Rift_Item"),
						};

					int bonusCount = 0;
					for (auto* b : bonusPool)
						if (b) ++bonusCount;

					if (bonusCount > 0)
					{
						int pick = RandRange(0, bonusCount - 1);
						for (auto* b : bonusPool)
						{
							if (!b)
								continue;
							if (pick-- == 0)
							{
								Satchel::TossFromContainer(b, RandRange(1, 3), spot);
								break;
							}
						}
					}

					static UFortItemDefinition* ammoPool[] = {
						Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium"),
						Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight"),
						Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy"),
						Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells"),
						};

					int ammoCount = 0;
					for (auto* a : ammoPool)
						if (a) ++ammoCount;

					if (ammoCount > 0)
					{
						int pick = RandRange(0, ammoCount - 1);
						for (auto* a : ammoPool)
						{
							if (!a)
								continue;
							if (pick-- == 0)
							{
								Satchel::TossFromContainer(a, RandRange(30, 60), spot);
								break;
							}
						}
					}

					return 1;
				};
				Patch::Attach(reinterpret_cast<void**>(&LootNative), loot);
				LootHooked = true;
			}
		}

		const bool ok = ReadyOriginal ? ReadyOriginal(mode) : true;
		KeepPlaylist();
		return ok;
	}

	inline void LaunchBus()
	{
		auto* mode = Mode();
		auto* state = State();
		if (!mode || !state)
			return;
		if (!Listening)
		{
			Note("listen is not up yet — wait for listening on {}", Cfg::ListenPort);
			return;
		}

		state->bGameModeWillSkipAircraft = false;
		state->AirCraftBehavior = EAirCraftBehavior::Default;
		if (auto* list = state->CurrentPlaylistInfo.BasePlaylist)
			Front::TuneList(list);

		if (state->GamePhase >= EAthenaGamePhase::EndGame || state->GamePhase < EAthenaGamePhase::Warmup)
		{
			const auto old = state->GamePhase;
			state->GamePhase = EAthenaGamePhase::Warmup;
			state->GamePhaseStep = EAthenaGamePhaseStep::Warmup;
			state->OnRep_GamePhase(old);
		}

		const float now = UGameplayStatics::GetTimeSeconds(World());
		state->WarmupCountdownEndTime = now + Cfg::WarmupSeconds;
		mode->WarmupCountdownDuration = Cfg::WarmupSeconds;
		state->WarmupCountdownStartTime = now;
		mode->WarmupEarlyCountdownDuration = Cfg::WarmupSeconds;
		if (kStormRush)
			Note("late warmup {}s then bus ({})", Cfg::WarmupSeconds, kFlavorName);
		else
			Note("bus countdown started");
	}
}
