#pragma once

#include "Pch.hpp"

namespace Revert::Front
{
	inline constexpr float kRingSpan = Cfg::ZoneSizes[0];
	inline constexpr float kBusHeight = Cfg::BusHeight;
	inline constexpr float kDropLock = Cfg::DropLock;
	inline constexpr float kBusLife = Cfg::BusLife;
	inline constexpr float kZoneHold = Cfg::ZoneHold;
	inline constexpr float kZoneShrink = Cfg::ZoneShrink;
	inline constexpr int kLateZone = Cfg::LateZone;

	inline FVector Center{};
	inline FAircraftFlightInfo Path{};
	inline bool SpotReady = false;
	inline bool PathReady = false;
	inline bool DropOpen = false;
	inline bool InPulse = false;
	inline bool HoldPulse = false;
	inline bool WaitNote = false;
	inline bool BusNote = false;
	inline bool RingShown = false;
	inline bool RingNote = false;
	inline bool ZoneBusy = false;
	inline bool ZoneLocked = false;
	inline int ZoneStep = 0;
	inline int RingWait = 0;
	inline bool BusGone = false;
	inline bool BusParked = false;
	inline bool StartedFlight = false;
	inline bool SpawnTried = false;
	inline int WaitTicks = 0;
	inline float BusBorn = 0.f;
	inline float ZoneBorn = 0.f;
	inline std::vector<AFortPlayerController*> Jumped;
	inline int (*BoardAlive)() = nullptr;
	inline void (*FlushBus)() = nullptr;
	inline bool (*AnyoneInBus)() = nullptr;

	inline FVector_NetQuantize100 AsNet(const FVector& v)
	{
		return FVector_NetQuantize100{ v.X, v.Y, v.Z };
	}

	inline void RegisterBus(AFortAthenaAircraft* bus)
	{
		if (!bus)
			return;

		auto addOnce = [&](TArray<AFortAthenaAircraft*>& list)
		{
			for (int i = 0; i < list.Num(); ++i)
			{
				if (list[i] == bus)
					return;
			}
			list.Add(bus);
		};

		if (auto* state = State())
		{
			addOnce(state->Aircrafts);
			state->OnRep_Aircraft();
		}
		if (auto* mode = Mode())
			addOnce(mode->Aircrafts);
	}

	inline AFortAthenaAircraft* Bus()
	{
		auto* state = State();
		if (state && state->Aircrafts.Num() && state->Aircrafts[0])
			return state->Aircrafts[0];
		auto* mode = Mode();
		if (mode && mode->Aircrafts.Num() && mode->Aircrafts[0])
			return mode->Aircrafts[0];
		return nullptr;
	}

	inline bool CanDrop()
	{
		return DropOpen;
	}

	inline void OpenDrop(AFortGameStateAthena* state)
	{
		if (!DropOpen)
			Note("drop open");
		DropOpen = true;
		if (state)
			state->bAircraftIsLocked = false;
	}

	inline void WipeActors(UClass* type)
	{
		if (!World() || !type)
			return;
		TArray<AActor*> found;
		UGameplayStatics::GetAllActorsOfClass(World(), type, &found);
		for (int i = 0; i < found.Num(); ++i)
		{
			if (found[i])
				found[i]->K2_DestroyActor();
		}
		found.Free();
	}

	inline void ClearAircraft()
	{
		WipeActors(AFortAthenaAircraft::StaticClass());
		if (auto* mode = Mode())
			mode->Aircrafts.Count = 0;
		if (auto* state = State())
		{
			state->Aircrafts.Count = 0;
			state->OnRep_Aircraft();
		}
	}

	inline void ClearRing()
	{
		WipeActors(AFortSafeZoneIndicator::StaticClass());
		if (auto* mode = Mode())
		{
			mode->SafeZoneIndicator = nullptr;
			mode->SafeZonePhase = 0;
			mode->bSafeZoneActive = false;
			mode->bSafeZonePaused = false;
		}
		if (auto* state = State())
		{
			state->SafeZoneIndicator = nullptr;
			state->SafeZonePhase = 0;
			state->bSafeZonePaused = false;
			state->bStormReachedFinalPosition = false;
			state->OnRep_SafeZoneIndicator();
			state->OnRep_SafeZonePhase();
		}
	}

	inline void ResetMatch()
	{
		ClearAircraft();
		ClearRing();
		DropOpen = false;
		InPulse = false;
		WaitNote = false;
		BusNote = false;
		RingShown = false;
		RingNote = false;
		ZoneBusy = false;
		ZoneLocked = false;
		ZoneStep = 0;
		RingWait = 0;
		BusGone = false;
		BusParked = false;
		StartedFlight = false;
		SpawnTried = false;
		WaitTicks = 0;
		BusBorn = 0.f;
		ZoneBorn = 0.f;
		PathReady = false;
		SpotReady = false;
		Jumped.clear();
	}

	inline void TuneList(UFortPlaylistAthena* list)
	{
		if (!kStormRush || !list)
			return;

		list->bSkipAircraft = false;
		list->bSkipWarmup = false;
		list->bAircraftDropOnlyWithinSafeZone = false;
		list->bWarmUpInStorm = false;
		list->bUseCustomAircraftPathSelection = false;
		list->AirCraftBehavior = EAirCraftBehavior::Default;
		list->SafeZoneStartUp = ESafeZoneStartUp::StartsWithAirCraft;
	}

	inline void PickSpot()
	{
		auto* mode = Mode();
		if (mode && mode->SafeZoneLocations.Num() >= kLateZone)
		{
			Center = mode->SafeZoneLocations[kLateZone - 1];
			Center.Z = 0.f;
			SpotReady = true;
			Note("late center zone {:.0f} {:.0f}", Center.X, Center.Y);
			return;
		}

		TArray<AActor*> foundations;
		if (World())
			UGameplayStatics::GetAllActorsOfClass(World(), ABuildingFoundation::StaticClass(), &foundations);
		if (foundations.Num())
		{
			Center = foundations[RandRange(0, foundations.Num() - 1)]->K2_GetActorLocation();
			Center.Z = 0.f;
			foundations.Free();
			SpotReady = true;
			Note("late center poi {:.0f} {:.0f}", Center.X, Center.Y);
			return;
		}
		foundations.Free();

		Center = FVector{
			static_cast<float>(RandRange(-50000, 50000)),
			static_cast<float>(RandRange(-50000, 50000)),
			0.f
		};
		SpotReady = true;
		Note("late center {:.0f} {:.0f}", Center.X, Center.Y);
	}

	inline void MakePath()
	{
		if (!SpotReady)
			PickSpot();

		const float yaw = static_cast<float>(RandRange(0, 359));
		const FRotator rot{ 0.f, yaw, 0.f };

		Path.FlightStartLocation = AsNet({ Center.X, Center.Y, kBusHeight });
		Path.FlightStartRotation = rot;
		Path.FlightSpeed = 0.f;
		Path.TimeTillDropStart = 0.f;
		Path.TimeTillDropEnd = 600.f;
		Path.TimeTillFlightEnd = 600.f;
		PathReady = true;
		Note("late bus in zone yaw {:.0f} drop in {:.0f}s gone in {:.0f}s", yaw, Path.TimeTillDropStart, kBusLife);
	}

	inline void PutPath(FAircraftFlightInfo& dest)
	{
		dest = Path;
	}

	inline void PlantPath()
	{
		if (!PathReady)
			MakePath();

		auto* state = State();
		if (!state || !state->MapInfo)
			return;

		auto* map = state->MapInfo;
		if (!map->FlightInfos.Num())
			map->FlightInfos.Add(Path);
		else
		{
			for (int i = 0; i < map->FlightInfos.Num(); ++i)
				PutPath(map->FlightInfos[i]);
		}

		if (!state->TeamFlightPaths.Num())
			state->TeamFlightPaths.Add(Path);
		else
		{
			for (int i = 0; i < state->TeamFlightPaths.Num(); ++i)
				PutPath(state->TeamFlightPaths[i]);
		}
		state->FlightPathMidLine = Path;
		state->OnRep_MapInfo();
	}

	inline void UnpinZeroSpeed()
	{
		auto* state = State();
		if (!state || !state->MapInfo)
			return;

		auto* map = state->MapInfo;
		for (int i = 0; i < map->FlightInfos.Num(); ++i)
		{
			if (map->FlightInfos[i].FlightSpeed <= 1.f)
				map->FlightInfos[i].FlightSpeed = 1500.f;
			if (map->FlightInfos[i].TimeTillFlightEnd <= 1.f)
				map->FlightInfos[i].TimeTillFlightEnd = 60.f;
			if (map->FlightInfos[i].TimeTillDropEnd <= 1.f)
				map->FlightInfos[i].TimeTillDropEnd = 50.f;
		}
	}

	inline void EnterAircraftPhase()
	{
		auto* state = State();
		if (!state || state->GamePhase >= EAthenaGamePhase::EndGame)
			return;
		if (state->GamePhase > EAthenaGamePhase::Warmup)
			return;

		const auto old = state->GamePhase;
		state->GamePhase = EAthenaGamePhase::Aircraft;
		state->GamePhaseStep = EAthenaGamePhaseStep::BusLocked;
		state->OnRep_GamePhase(old);
	}

	inline void StartAircraft()
	{
		auto* state = State();
		auto* mode = Mode();
		if (!state || !mode || !World())
			return;

		state->bGameModeWillSkipAircraft = false;
		state->AirCraftBehavior = EAirCraftBehavior::Default;
		state->CachedSafeZoneStartUp = ESafeZoneStartUp::StartsWithAirCraft;
		if (auto* list = state->CurrentPlaylistInfo.BasePlaylist)
			TuneList(list);

		UKismetSystemLibrary::ExecuteConsoleCommand(World(), L"startaircraft", nullptr);
		Note("late startaircraft phase {}", static_cast<int>(state->GamePhase));
	}

	inline AFortAthenaAircraft* SpawnBus()
	{
		auto* state = State();
		if (!state || !state->MapInfo || !World())
			return nullptr;
		if (!PathReady)
			MakePath();

		auto* klass = state->MapInfo->AircraftClass;
		if (!klass)
			klass = AFortAthenaAircraft::StaticClass();

		const FVector loc{ Center.X, Center.Y, kBusHeight };
		auto* bus = World()->SpawnActor<AFortAthenaAircraft>(loc, Path.FlightStartRotation, klass);
		if (!bus)
		{
			Note("late bus spawn failed");
			return nullptr;
		}

		RegisterBus(bus);
		EnterAircraftPhase();
		Note("late bus spawned at {:.0f} {:.0f}", Center.X, Center.Y);
		return bus;
	}

	inline void PlantZone()
	{
		auto* mode = Mode();
		if (!mode || !SpotReady)
			return;

		if (!mode->SafeZoneLocations.Num())
		{
			for (int i = 0; i < 8; ++i)
				mode->SafeZoneLocations.Add(Center);
		}
		else
		{
			for (int i = 0; i < mode->SafeZoneLocations.Num(); ++i)
				mode->SafeZoneLocations[i] = Center;
		}
		mode->bSafeZoneLocationsInitialized = true;
		mode->bSafeZoneActive = true;

		if (auto* state = State())
			state->DropZoneCenter = FVector2D{ Center.X, Center.Y };
	}

	inline bool HasJumped(AFortPlayerController* pc)
	{
		if (!pc)
			return false;
		for (auto* who : Jumped)
		{
			if (who == pc)
				return true;
		}
		return false;
	}

	inline void MarkJumped(AFortPlayerController* pc)
	{
		if (pc && !HasJumped(pc))
			Jumped.push_back(pc);
	}

	inline void ArmZoneNow()
	{
		auto* state = State();
		if (!state || !World())
			return;
		state->SafeZonesStartTime = UGameplayStatics::GetTimeSeconds(World());
	}

	inline void FitRing(AFortSafeZoneIndicator* ring, float lastR, float nextR, float start, float finish)
	{
		if (!ring || !SpotReady)
			return;

		const float now = World() ? UGameplayStatics::GetTimeSeconds(World()) : 0.f;
		float current = lastR;
		if (finish > start && now >= finish)
			current = nextR;
		else if (finish > start && now > start)
		{
			float a = (now - start) / (finish - start);
			if (a < 0.f) a = 0.f;
			if (a > 1.f) a = 1.f;
			current = lastR + (nextR - lastR) * a;
		}

		PlantZone();
		const auto loc = AsNet(Center);
		ring->Radius = current;
		ring->LastRadius = lastR;
		ring->NextRadius = nextR;
		ring->NextNextRadius = nextR;
		ring->LastCenter = loc;
		ring->NextCenter = loc;
		ring->NextNextCenter = loc;
		ring->SafeZoneStartShrinkTime = start;
		ring->SafeZoneFinishShrinkTime = finish;
		ring->HoldingStartTime = start;
		ring->SetSafeZoneRadiusAndCenter(current, Center);

		FHitResult hit{};
		ring->K2_SetActorLocation(Center, false, true, &hit);
	}

	inline AFortSafeZoneIndicator* Ring()
	{
		auto* state = State();
		if (state && state->SafeZoneIndicator)
			return state->SafeZoneIndicator;
		auto* mode = Mode();
		return mode ? mode->SafeZoneIndicator : nullptr;
	}

	inline void PaintZone()
	{
		auto* ring = Ring();
		if (!ring || !RingShown || !World())
			return;

		const float now = UGameplayStatics::GetTimeSeconds(World());
		if (ZoneBorn <= 0.f)
			ZoneBorn = now;

		static const float sizes[] = {
			Cfg::ZoneSizes[0],
			Cfg::ZoneSizes[1],
			Cfg::ZoneSizes[2],
			Cfg::ZoneSizes[3],
			Cfg::ZoneSizes[4]
		};
		const float elapsed = now - ZoneBorn;
		float acc = 0.f;
		for (int i = 0; i < 4; ++i)
		{
			const float last = sizes[i];
			const float next = sizes[i + 1];
			if (elapsed < acc + kZoneHold)
			{
				FitRing(ring, last, next, ZoneBorn + acc + kZoneHold, ZoneBorn + acc + kZoneHold + kZoneShrink);
				return;
			}
			acc += kZoneHold;
			if (elapsed < acc + kZoneShrink)
			{
				FitRing(ring, last, next, ZoneBorn + acc, ZoneBorn + acc + kZoneShrink);
				return;
			}
			acc += kZoneShrink;
		}
		FitRing(ring, 0.f, 0.f, now - 1.f, now - 1.f);
	}

	inline void ShowRing()
	{
		if (!World() || !BusParked)
			return;

		auto* state = State();
		if (!state)
			return;

		if (!RingShown)
		{
			PlantZone();
			UKismetSystemLibrary::ExecuteConsoleCommand(World(), L"startsafezone", nullptr);
			ArmZoneNow();
			RingShown = true;
			ZoneBorn = UGameplayStatics::GetTimeSeconds(World());
			ZoneLocked = true;
		}

		auto* ring = Ring();
		if (ring && !RingNote)
		{
			RingNote = true;
			Note("late zone pinned {:.0f} {:.0f}", Center.X, Center.Y);
		}
		else if (RingShown && !ring && ++RingWait == 45)
		{
			PlantZone();
			UKismetSystemLibrary::ExecuteConsoleCommand(World(), L"startsafezone", nullptr);
			ArmZoneNow();
		}

		PaintZone();
	}

	inline void Layout()
	{
		if (!kStormRush)
			return;

		auto* state = State();
		if (!state || !state->MapInfo)
			return;

		ResetMatch();
		PickSpot();
		PlantZone();
		MakePath();

		state->bGameModeWillSkipAircraft = false;
		state->AirCraftBehavior = EAirCraftBehavior::Default;
		state->CachedSafeZoneStartUp = ESafeZoneStartUp::StartsWithAirCraft;
		if (auto* list = state->CurrentPlaylistInfo.BasePlaylist)
			TuneList(list);
		Note("late game layout ({})", kFlavorName);
	}

	inline bool MatchLive()
	{
		auto* state = State();
		if (!state || !World())
			return false;
		if (state->GamePhase >= EAthenaGamePhase::EndGame)
			return false;
		if (state->GamePhase >= EAthenaGamePhase::Aircraft)
			return true;
		const float now = UGameplayStatics::GetTimeSeconds(World());
		return state->WarmupCountdownEndTime > 1.f && now >= state->WarmupCountdownEndTime;
	}

	inline void OnPhase(AFortSafeZoneIndicator*, void*)
	{
	}

	inline void ParkBus(AFortAthenaAircraft* bus)
	{
		if (!bus || BusParked)
			return;

		if (!PathReady)
			MakePath();

		bus->FlightInfo = Path;
		bus->FlightInfo.FlightSpeed = 0.f;
		const FVector loc{ Center.X, Center.Y, kBusHeight };
		FHitResult hit{};
		bus->K2_SetActorLocationAndRotation(loc, Path.FlightStartRotation, false, true, &hit);

		const float now = UGameplayStatics::GetTimeSeconds(World());
		bus->FlightStartTime = 0.f;
		bus->DropStartTime = 0.f;
		bus->DropEndTime = 999999.f;
		bus->FlightEndTime = 999999.f;
		if (auto* state = State())
		{
			state->DefaultParachuteDeployTraceForGroundDistance = 2500.f;
			OpenDrop(state);
		}

		PlantPath();
		RegisterBus(bus);
		BusBorn = now;
		BusParked = true;
		Note("late bus parked in zone {:.0f} {:.0f}", Center.X, Center.Y);
	}

	inline void KeepBusTimes(AFortAthenaAircraft* bus)
	{
		if (!bus || BusBorn <= 0.f)
			return;
		bus->FlightInfo.FlightSpeed = 0.f;
		bus->FlightInfo.TimeTillDropStart = 0.f;
		bus->FlightInfo.TimeTillDropEnd = 600.f;
		bus->FlightInfo.TimeTillFlightEnd = 600.f;
		bus->FlightStartTime = 0.f;
		bus->DropStartTime = 0.f;
		bus->DropEndTime = 999999.f;
		bus->FlightEndTime = 999999.f;
	}

	inline void KeepBusInZone(AFortAthenaAircraft* bus)
	{
		if (!bus || BusGone)
			return;

		bus->FlightInfo.FlightSpeed = 0.f;
		const FVector here = bus->K2_GetActorLocation();
		const float dx = here.X - Center.X;
		const float dy = here.Y - Center.Y;
		const float dz = here.Z > kBusHeight ? here.Z - kBusHeight : kBusHeight - here.Z;
		if ((dx * dx) + (dy * dy) > (4000.f * 4000.f) || dz > 8000.f)
		{
			FHitResult hit{};
			bus->K2_SetActorLocation({ Center.X, Center.Y, kBusHeight }, false, true, &hit);
		}
	}

	inline void HideBus()
	{
		if (auto* bus = Bus())
		{
			bus->SetActorHiddenInGame(true);
			if (bus->SpawnedCosmeticActor)
				bus->SpawnedCosmeticActor->SetActorHiddenInGame(true);
		}
	}

	inline void Pulse()
	{
		if (!kStormRush || InPulse || HoldPulse)
			return;

		InPulse = true;
		auto* state = State();
		if (!state)
		{
			InPulse = false;
			return;
		}

		if (state->GamePhase >= EAthenaGamePhase::EndGame)
		{
			InPulse = false;
			return;
		}

		if (!PathReady)
			Layout();

		state->bGameModeWillSkipAircraft = false;

		if (!MatchLive() || BusGone)
		{
			InPulse = false;
			return;
		}

		if (!StartedFlight)
		{
			StartedFlight = true;
			WaitTicks = 0;
			if (auto* mode = Mode(); mode && mode->SafeZoneLocations.Num() >= kLateZone)
			{
				PickSpot();
				PathReady = false;
				MakePath();
			}
			PlantZone();
			StartAircraft();
			InPulse = false;
			return;
		}

		auto* bus = Bus();
		if (!bus)
		{
			DropOpen = false;
			if (!SpawnTried && ++WaitTicks >= 15)
			{
				SpawnTried = true;
				bus = SpawnBus();
			}

			if (!bus)
			{
				if (!WaitNote)
				{
					WaitNote = true;
					Note("waiting for bus  phase {}", static_cast<int>(state->GamePhase));
				}
				InPulse = false;
				return;
			}
		}

		const float now = UGameplayStatics::GetTimeSeconds(World());
		ParkBus(bus);
		KeepBusInZone(bus);
		if (BusBorn <= 0.f)
			BusBorn = now;
		KeepBusTimes(bus);

		if (!BusNote)
		{
			BusNote = true;
			Note("late bus flying  drop at {}", bus->DropStartTime);
		}

		if (BoardAlive)
			BoardAlive();

		ShowRing();

		if (BusParked && !BusGone)
			OpenDrop(state);

		if (BusBorn > 0.f && now >= BusBorn + kBusLife)
		{
			BusGone = true;
			if (FlushBus)
				FlushBus();
			HideBus();
			Note("bus gone");
		}

		InPulse = false;
	}
}
