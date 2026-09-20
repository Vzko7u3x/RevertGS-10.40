#pragma once

#include "Pch.hpp"
#include "Patch.hpp"
#include "Satchel.hpp"
#include "Scene.hpp"
#include "Specs.hpp"

namespace Revert::Bots
{
	inline constexpr int kMatchSize = Cfg::MaxPlayers;
	inline constexpr int kMaxSpawnPerTick = Cfg::BotSpawnPerTick;

	enum class EBotState : uint8_t
	{
		Warmup,
		PreBus,
		Bus,
		Skydiving,
		Landed
	};

	struct PlayerBot
	{
		AFortPlayerPawnAthena* Pawn = nullptr;
		AFortAthenaAIBotController* PC = nullptr;
		AFortPlayerStateAthena* PS = nullptr;
		FVector WalkTo{};
		FVector DropZone{};
		float ClosestToDrop = 1.0e30f;
		int Tick = 0;
		int BusHold = 0;
		bool Jumped = false;
		EBotState State = EBotState::Warmup;
	};

	inline std::vector<PlayerBot*> All;
	inline uint8_t NextBotTeam = 0;
	inline int BotsOnTeam = 0;
	inline bool Hooked = false;
	inline bool LoggedSpawn = false;
	inline bool LoggedFill = false;
	inline bool LoggedJump = false;
	inline bool LoggedBus = false;
	inline bool TriedGoal = false;
	inline bool EverSpawned = false;

	using SpawnBotFn = AFortPlayerPawnAthena* (*)(UFortServerBotManagerAthena*, FVector, FRotator, UFortAthenaAIBotCustomizationData*);
	using AircraftExitFn = void (*)(AFortGameModeAthena*, AFortAthenaAircraft*);
	using AircraftEnterFn = __int64 (*)(AFortGameModeAthena*, AFortAthenaAircraft*);

	inline SpawnBotFn SpawnBotOriginal = Rva::Rel<SpawnBotFn>(Rva::SpawnBot);
	inline AircraftExitFn AircraftExitOriginal = Rva::Rel<AircraftExitFn>(Rva::AircraftExit);
	inline AircraftEnterFn AircraftEnterOriginal = Rva::Rel<AircraftEnterFn>(Rva::AircraftEnter);

	inline UFunction* AiFn(const char* path)
	{
		return UObject::FindObject<UFunction>(path);
	}

	inline void CallAi(UObject* object, UFunction* function, void* params)
	{
		if (!object || !function)
			return;
		const auto flags = function->FunctionFlags;
		function->FunctionFlags |= 0x400;
		object->ProcessEvent(function, params);
		function->FunctionFlags = flags;
	}

	inline bool RunTree(AAIController* pc, UBehaviorTree* tree)
	{
		if (!pc || !tree)
			return false;
		static auto* fn = AiFn("Function AIModule.AIController.RunBehaviorTree");
		AAIController_RunBehaviorTree_Params params{};
		params.BTAsset = tree;
		CallAi(pc, fn, &params);
		return params.ReturnValue;
	}

	inline EPathFollowingStatus MoveStatus(AAIController* pc)
	{
		if (!pc)
			return EPathFollowingStatus::Idle;
		static auto* fn = AiFn("Function AIModule.AIController.GetMoveStatus");
		AAIController_GetMoveStatus_Params params{};
		CallAi(pc, fn, &params);
		return params.ReturnValue;
	}

	inline void MoveTo(AAIController* pc, const FVector& dest, float radius = 80.f)
	{
		if (!pc)
			return;
		static auto* fn = AiFn("Function AIModule.AIController.MoveToLocation");
		AAIController_MoveToLocation_Params params{};
		params.Dest = dest;
		params.AcceptanceRadius = radius;
		params.bStopOnOverlap = true;
		params.bUsePathfinding = true;
		params.bProjectDestinationToNavigation = false;
		params.bCanStrafe = true;
		params.FilterClass = nullptr;
		params.bAllowPartialPath = true;
		CallAi(pc, fn, &params);
	}

	inline void ResetAll()
	{
		for (auto* bot : All)
			delete bot;
		All.clear();
		NextBotTeam = 0;
		BotsOnTeam = 0;
		LoggedSpawn = false;
		LoggedFill = false;
		LoggedJump = false;
		LoggedBus = false;
		TriedGoal = false;
		EverSpawned = false;
	}

	inline void WatchWorld()
	{
		static UWorld* last = nullptr;
		auto* world = World();
		if (world != last)
		{
			last = world;
			ResetAll();
		}
	}

	inline bool IsHumanController(AFortPlayerControllerAthena* pc)
	{
		if (!pc || !pc->PlayerState || pc->PlayerState->bIsABot)
			return false;
		return As<AFortAthenaAIBotController>(pc) == nullptr;
	}

	inline int AliveHumans()
	{
		auto* mode = Mode();
		if (!mode)
			return 0;
		int n = 0;
		for (int i = 0; i < mode->AlivePlayers.Num(); ++i)
		{
			if (IsHumanController(mode->AlivePlayers[i]))
				++n;
		}
		return n;
	}

	inline int ConnectionHumans()
	{
		auto* world = World();
		if (!world || !world->NetDriver)
			return 0;
		int n = 0;
		for (int i = 0; i < world->NetDriver->ClientConnections.Num(); ++i)
		{
			auto* conn = world->NetDriver->ClientConnections[i];
			auto* pc = conn ? As<AFortPlayerControllerAthena>(conn->PlayerController) : nullptr;
			if (IsHumanController(pc))
				++n;
		}
		return n;
	}

	inline int HumanCount()
	{
		const int fromAlive = AliveHumans();
		const int fromConn = ConnectionHumans();
		return fromAlive > fromConn ? fromAlive : fromConn;
	}

	inline int LivingBots()
	{
		int n = 0;
		for (auto* bot : All)
		{
			if (bot && bot->Pawn && !bot->Pawn->IsDead())
				++n;
		}
		return n;
	}

	inline void SyncPlayersLeft()
	{
		auto* state = State();
		if (!state)
			return;
		const int alive = HumanCount() + LivingBots();
		if (alive <= 0 || state->PlayersLeft == alive)
			return;
		state->PlayersLeft = alive;
		state->OnRep_PlayersLeft();
	}

	inline void KeepWalking(AFortPlayerPawnAthena* pawn)
	{
		if (!pawn || !pawn->CharacterMovement)
			return;
		pawn->CharacterMovement->GravityScale = 1.f;
		pawn->CharacterMovement->SetMovementMode(EMovementMode::MOVE_Walking, 0);
	}

	inline UFortItemDefinition* Item(const char* path)
	{
		return Find<UFortItemDefinition>(path);
	}

	inline void Give(AFortAthenaAIBotController* pc, UFortItemDefinition* def, int count = 1, int ammo = 0)
	{
		if (!pc || !pc->Inventory || !def || count <= 0)
			return;
		auto* item = As<UFortWorldItem>(def->CreateTemporaryItemInstanceBP(count, 0));
		if (!item)
			return;
		item->OwnerInventory = pc->Inventory;
		item->ItemEntry.Count = count;
		item->ItemEntry.LoadedAmmo = ammo;
		pc->Inventory->Inventory.ItemInstances.Add(item);
		auto& entry = pc->Inventory->Inventory.ReplicatedEntries.Add(item->ItemEntry);
		entry.LoadedAmmo = ammo;
		pc->Inventory->Inventory.MarkItemDirty(entry);
		pc->Inventory->HandleInventoryLocalUpdate();
	}

	inline void EquipPickaxe(PlayerBot* bot)
	{
		if (!bot || !bot->Pawn || !bot->PC || !bot->PC->Inventory)
			return;
		auto& rows = bot->PC->Inventory->Inventory.ReplicatedEntries;
		for (int i = 0; i < rows.Num(); ++i)
		{
			if (rows[i].ItemDefinition && rows[i].ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
			{
				bot->Pawn->EquipWeaponDefinition(static_cast<UFortWeaponItemDefinition*>(rows[i].ItemDefinition), rows[i].ItemGuid);
				return;
			}
		}
	}

	inline void EnsureInventory(AFortAthenaAIBotController* pc, AFortPlayerPawnAthena* pawn)
	{
		if (!pc || pc->Inventory || !World())
			return;
		auto* inv = World()->SpawnActor<AFortInventory>(pawn ? pawn->K2_GetActorLocation() : FVector{});
		if (!inv)
			return;
		inv->Owner = pc;
		inv->OnRep_Owner();
		pc->Inventory = inv;
	}

	inline void GiveStartup(AFortAthenaAIBotController* pc, UFortAthenaAIBotCustomizationData* data)
	{
		if (!pc || !pc->Inventory || !data || !data->StartupInventory)
			return;
		for (int i = 0; i < data->StartupInventory->Items.Num(); ++i)
			Give(pc, data->StartupInventory->Items[i], 1, 0);
	}

	inline void EnsureManager()
	{
		auto* mode = Mode();
		auto* state = State();
		auto* world = World();
		if (!mode || !state || !world)
			return;

		if (auto* nav = As<UNavigationSystemV1>(world->NavigationSystem))
			nav->bAutoCreateNavigationData = 1;

		if (!mode->ServerBotManager)
		{
			auto* manager = static_cast<UFortServerBotManagerAthena*>(UGameplayStatics::SpawnObject(UFortServerBotManagerAthena::StaticClass(), mode));
			if (!manager)
				return;
			manager->CachedGameMode = mode;
			manager->CachedGameState = state;
			manager->bBotInfiniteAmmo = true;
			manager->bBotInfiniteResources = false;
			manager->bBotHostileToHumanPlayersOnly = false;
			mode->ServerBotManager = manager;
		}

		if (mode->ServerBotManager && !mode->ServerBotManager->CachedBotMutator)
		{
			auto* mutator = world->SpawnActor<AFortAthenaMutator_Bots>(FVector{});
			if (mutator)
			{
				mutator->CachedGameMode = mode;
				mutator->CachedGameState = state;
				mode->ServerBotManager->CachedBotMutator = mutator;
			}
		}

		if (!mode->AIGoalManager && !TriedGoal)
		{
			TriedGoal = true;
			mode->AIGoalManager = world->SpawnActor<AFortAIGoalManager>(FVector{});
		}
	}

	inline UFortAthenaAIBotCustomizationData* MakeData()
	{
		auto* mode = Mode();
		if (!mode)
			return nullptr;

		static auto* pawnClass = Find<UBlueprintGeneratedClass>("/Game/Athena/AI/Phoebe/BP_PlayerPawn_Athena_Phoebe.BP_PlayerPawn_Athena_Phoebe_C");
		static auto* behavior = Find<UBehaviorTree>("/Game/Athena/AI/Phoebe/BehaviorTrees/BT_Phoebe.BT_Phoebe");
		auto* data = static_cast<UFortAthenaAIBotCustomizationData*>(UGameplayStatics::SpawnObject(UFortAthenaAIBotCustomizationData::StaticClass(), mode));
		if (!data)
			return nullptr;

		data->PawnClass = pawnClass;
		data->BehaviorTree = behavior;
		data->bOverrideBehaviorTree = behavior != nullptr;
		data->bOverrideCharacterCustomization = false;
		data->bOverrideDBNOPlayStyle = false;
		data->bOverrideSkillLevel = false;
		data->bOverrideSkillSets = false;
		data->bOverrideStartupInventory = false;
		data->DBNOPlayStyle = EDBNOPlayStyle::Default;
		data->SkillLevel = 1.f;
		data->StartupInventory = static_cast<UFortAthenaAIBotInventoryItems*>(UGameplayStatics::SpawnObject(UFortAthenaAIBotInventoryItems::StaticClass(), mode));
		if (data->StartupInventory)
		{
			if (auto* pick = Item("/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01"))
				data->StartupInventory->Items.Add(pick);
			if (auto* floor = Item("/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor"))
				data->StartupInventory->Items.Add(floor);
			if (auto* roof = Item("/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS"))
				data->StartupInventory->Items.Add(roof);
			if (auto* stair = Item("/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W"))
				data->StartupInventory->Items.Add(stair);
			if (auto* wall = Item("/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall"))
				data->StartupInventory->Items.Add(wall);
			if (auto* edit = Item("/Game/Items/Weapons/BuildingTools/EditTool.EditTool"))
				data->StartupInventory->Items.Add(edit);
		}
		return data;
	}

	inline void TrackAlive(AFortAthenaAIBotController* pc)
	{
		auto* mode = Mode();
		if (!mode || !pc)
			return;
		for (int i = 0; i < mode->AliveBots.Num(); ++i)
		{
			if (mode->AliveBots[i] == pc)
				return;
		}
		mode->AliveBots.Add(pc);
	}

	inline void AssignTeam(AFortPlayerStateAthena* ps)
	{
		auto* state = State();
		if (!ps || !state)
			return;

		auto* list = state->CurrentPlaylistInfo.BasePlaylist;
		if (!NextBotTeam)
			NextBotTeam = list ? list->DefaultLastTeam : 50;

		const int squad = list && list->MaxSquadSize > 0 ? list->MaxSquadSize : 1;
		ps->TeamIndex = NextBotTeam;
		ps->OnRep_TeamIndex(0);
		ps->SquadId = static_cast<uint8_t>(ps->TeamIndex - 3);
		ps->OnRep_SquadId();
		ps->OnRep_PlayerTeam();
		ps->OnRep_PlayerTeamPrivate();

		++BotsOnTeam;
		if (BotsOnTeam >= squad)
		{
			++NextBotTeam;
			BotsOnTeam = 0;
		}

		FGameMemberInfo member{};
		member.ReplicationID = -1;
		member.ReplicationKey = -1;
		member.MostRecentArrayReplicationKey = -1;
		member.TeamIndex = ps->TeamIndex;
		member.SquadId = ps->SquadId;
		member.MemberUniqueId = ps->GetUniqueID();
		auto& added = state->GameMemberInfoArray.Members.Add(member);
		state->GameMemberInfoArray.MarkItemDirty(added);
	}

	inline AFortPlayerPawnAthena* OnSpawnBot(UFortServerBotManagerAthena* manager, FVector loc, FRotator rot, UFortAthenaAIBotCustomizationData* data)
	{
		auto* pawn = SpawnBotOriginal(manager, loc, rot, data);
		if (!kSolo || !pawn)
			return pawn;

		auto* pc = As<AFortAthenaAIBotController>(pawn->Controller);
		if (!pc && World())
		{
			pc = World()->SpawnActor<AFortAthenaAIBotController>(loc, rot);
			if (pc)
			{
				pc->bWantsPlayerState = 1;
				pc->Possess(pawn);
			}
		}
		auto* ps = pc ? As<AFortPlayerStateAthena>(pc->PlayerState) : nullptr;
		if (!pc || !ps)
			return pawn;

		pc->PlayerBotPawn = pawn;
		pc->CachedBotManager = manager;
		pc->CachedGameMode = Mode();
		if (data && data->BehaviorTree)
		{
			pc->BehaviorTree = data->BehaviorTree;
			RunTree(pc, data->BehaviorTree);
		}

		ps->bIsABot = 1;
		ps->bHasStartedPlaying = true;
		ps->OnRep_bHasStartedPlaying();
		KeepWalking(pawn);
		pawn->SetMaxHealth(100.f);
		pawn->SetHealth(100.f);
		pawn->SetMaxShield(100.f);
		pawn->SetShield(0.f);
		Specs::SeedPlayer(ps);

		EnsureInventory(pc, pawn);
		GiveStartup(pc, data);
		TrackAlive(pc);

		auto* bot = new PlayerBot{};
		bot->Pawn = pawn;
		bot->PC = pc;
		bot->PS = ps;
		bot->State = EBotState::Warmup;
		All.push_back(bot);
		EquipPickaxe(bot);
		return pawn;
	}

	inline AActor* WarmupStart()
	{
		if (!World())
			return nullptr;
		TArray<AActor*> starts;
		UGameplayStatics::GetAllActorsOfClass(World(), AFortPlayerStartWarmup::StaticClass(), &starts);
		AActor* pick = nullptr;
		if (starts.Num())
			pick = starts[RandRange(0, starts.Num() - 1)];
		starts.Free();
		return pick;
	}

	inline FVector RandomIslandSpot()
	{
		if (auto* start = WarmupStart())
		{
			auto loc = start->K2_GetActorLocation();
			loc.X += static_cast<float>(RandRange(-800, 800));
			loc.Y += static_cast<float>(RandRange(-800, 800));
			return loc;
		}
		TArray<AActor*> slabs;
		UGameplayStatics::GetAllActorsOfClass(World(), ABuildingFoundation::StaticClass(), &slabs);
		FVector loc{};
		if (slabs.Num())
			loc = slabs[RandRange(0, slabs.Num() - 1)]->K2_GetActorLocation();
		slabs.Free();
		return loc;
	}

	inline bool SpawnOne()
	{
		auto* mode = Mode();
		auto* state = State();
		if (!mode || !state || !mode->ServerBotManager)
			return false;

		auto* start = WarmupStart();
		if (!start)
			return false;

		auto* data = MakeData();
		if (!data)
			return false;

		const int before = static_cast<int>(All.size());
		auto* pawn = mode->ServerBotManager->SpawnBot(start->K2_GetActorLocation(), start->K2_GetActorRotation(), data);
		if (!pawn)
			return false;

		PlayerBot* bot = nullptr;
		for (int i = static_cast<int>(All.size()) - 1; i >= before; --i)
		{
			if (All[static_cast<size_t>(i)] && All[static_cast<size_t>(i)]->Pawn == pawn)
			{
				bot = All[static_cast<size_t>(i)];
				break;
			}
		}
		if (!bot)
		{
			for (auto* it : All)
			{
				if (it && it->Pawn == pawn)
				{
					bot = it;
					break;
				}
			}
		}
		if (!bot || !bot->PS)
			return false;

		AssignTeam(bot->PS);
		SyncPlayersLeft();
		EverSpawned = true;
		if (!LoggedSpawn)
		{
			LoggedSpawn = true;
			Note("bot ready  team {}  players {}", bot->PS->TeamIndex, state->PlayersLeft);
		}
		return true;
	}

	inline void Fill()
	{
		auto* mode = Mode();
		auto* state = State();
		if (!mode || !state)
			return;
		if (state->GamePhase != EAthenaGamePhase::Warmup)
			return;

		const int humans = HumanCount();
		if (humans <= 0)
			return;

		int want = kMatchSize - humans;
		if (want < 0)
			want = 0;
		const int have = LivingBots();
		if (have >= want)
		{
			if (!LoggedFill)
			{
				LoggedFill = true;
				Note("lobby filled  humans {}  bots {}  players {}", humans, have, humans + have);
			}
			return;
		}

		int need = want - have;
		if (need > kMaxSpawnPerTick)
			need = kMaxSpawnPerTick;
		for (int i = 0; i < need; ++i)
		{
			if (!SpawnOne())
				break;
		}
	}

	inline void PickWalk(PlayerBot* bot)
	{
		if (!bot || !bot->Pawn)
			return;
		auto here = bot->Pawn->K2_GetActorLocation();
		bot->WalkTo = FVector{
			here.X + static_cast<float>(RandRange(-2500, 2500)),
			here.Y + static_cast<float>(RandRange(-2500, 2500)),
			here.Z
		};
	}

	inline void TickWalk(PlayerBot* bot)
	{
		if (!bot || !bot->Pawn || !bot->PC)
			return;
		KeepWalking(bot->Pawn);
		if (bot->WalkTo.X == 0.f && bot->WalkTo.Y == 0.f && bot->WalkTo.Z == 0.f)
			PickWalk(bot);

		FVector here = bot->Pawn->K2_GetActorLocation();
		const float dist = UKismetMathLibrary::Vector_Distance(here, bot->WalkTo);
		if (dist < 180.f || (bot->Tick % 90) == 0)
			PickWalk(bot);

		if (MoveStatus(bot->PC) == EPathFollowingStatus::Idle || (bot->Tick % 45) == 1)
			MoveTo(bot->PC, bot->WalkTo);

		FVector delta = bot->WalkTo - here;
		if (dist > 40.f)
		{
			FVector dir{ delta.X / dist, delta.Y / dist, 0.f };
			bot->Pawn->AddMovementInput(dir, 1.f, true);
			auto look = UKismetMathLibrary::FindLookAtRotation(here, bot->WalkTo);
			look.Pitch = 0.f;
			look.Roll = 0.f;
			bot->PC->SetControlRotation(look);
			bot->PC->K2_SetActorRotation(look, true);
		}
	}

	inline AFortAthenaAircraft* FindBus()
	{
		auto* state = State();
		if (state)
		{
			if (auto* aircraft = state->GetAircraft(0))
				return aircraft;
			if (state->Aircrafts.Num() && state->Aircrafts[0])
				return state->Aircrafts[0];
		}
		auto* mode = Mode();
		if (mode && mode->Aircrafts.Num() && mode->Aircrafts[0])
			return mode->Aircrafts[0];
		return nullptr;
	}

	inline bool InTheAir(AFortPlayerPawnAthena* pawn)
	{
		if (!pawn)
			return false;
		return pawn->IsSkydiving() || pawn->IsParachuteOpen() || pawn->IsSkydivingFromBus();
	}

	inline void JumpFromBus(PlayerBot* bot)
	{
		if (!bot || !bot->Pawn || bot->Jumped)
			return;
		auto* aircraft = FindBus();
		if (aircraft)
			bot->Pawn->K2_TeleportTo(aircraft->K2_GetActorLocation(), aircraft->K2_GetActorRotation());
		if (bot->Pawn->CharacterMovement)
			bot->Pawn->CharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling, 0);
		bot->Pawn->BeginSkydiving(true);
		bot->Jumped = true;
		bot->State = EBotState::Skydiving;
		if (bot->PS)
		{
			bot->PS->bInAircraft = 0;
			bot->PS->bHasEverSkydivedFromBus = true;
		}
		if (!LoggedJump)
		{
			LoggedJump = true;
			Note("bot jumped from bus");
		}
	}

	inline void TickBus(PlayerBot* bot)
	{
		if (!bot || !bot->Pawn || !bot->PC || bot->Jumped)
			return;

		auto* state = State();
		if (!state)
			return;

		bot->PC->StopMovement();

		if (bot->DropZone.X == 0.f && bot->DropZone.Y == 0.f && bot->DropZone.Z == 0.f)
			bot->DropZone = RandomIslandSpot();

		auto* aircraft = FindBus();
		if (!aircraft)
		{
			bot->State = EBotState::PreBus;
			if (state->GamePhase > EAthenaGamePhase::Aircraft)
				JumpFromBus(bot);
			return;
		}

		bot->State = EBotState::Bus;
		bot->Pawn->K2_TeleportTo(aircraft->K2_GetActorLocation(), aircraft->K2_GetActorRotation());
		if (bot->PS)
			bot->PS->bInAircraft = 1;

		++bot->BusHold;
		if (bot->BusHold < 8)
			return;

		if (state->GamePhase > EAthenaGamePhase::Aircraft)
		{
			JumpFromBus(bot);
			return;
		}

		FVector busLoc = aircraft->K2_GetActorLocation();
		FVector target = bot->DropZone;
		target.Z = busLoc.Z;
		const float dist = UKismetMathLibrary::Vector_Distance(busLoc, target);
		if (dist < bot->ClosestToDrop)
			bot->ClosestToDrop = dist;
		else if (UKismetMathLibrary::RandomBoolWithWeight(0.75f))
			JumpFromBus(bot);
		else if (bot->BusHold > 90)
			JumpFromBus(bot);
	}

	inline void TickGround(PlayerBot* bot)
	{
		if (!bot || !bot->Pawn || !bot->PC)
			return;
		if (InTheAir(bot->Pawn) || bot->Pawn->K2_GetActorLocation().Z > 8000.f)
		{
			bot->State = EBotState::Skydiving;
			if (!bot->Pawn->IsSkydiving() && !bot->Pawn->IsParachuteOpen())
			{
				if (bot->Pawn->CharacterMovement)
					bot->Pawn->CharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling, 0);
				bot->Pawn->BeginSkydiving(true);
			}
			return;
		}
		bot->State = EBotState::Landed;
		KeepWalking(bot->Pawn);
		if (bot->DropZone.X == 0.f && bot->DropZone.Y == 0.f)
			bot->DropZone = RandomIslandSpot();
		bot->WalkTo = bot->DropZone;
		TickWalk(bot);
		EquipPickaxe(bot);
	}

	inline void TickOne(PlayerBot* bot)
	{
		if (!bot || !bot->PC || !bot->Pawn || !bot->PS)
			return;
		if (bot->Pawn->IsDead())
			return;

		++bot->Tick;
		auto* state = State();
		if (!state)
			return;

		if (state->GamePhase == EAthenaGamePhase::Warmup)
		{
			bot->State = EBotState::Warmup;
			if (bot->Tick <= 30)
			{
				KeepWalking(bot->Pawn);
				return;
			}
			TickWalk(bot);
			return;
		}

		if (!bot->Jumped)
		{
			TickBus(bot);
			return;
		}

		if (InTheAir(bot->Pawn) || bot->Pawn->K2_GetActorLocation().Z > 8000.f)
		{
			bot->State = EBotState::Skydiving;
			if (!bot->Pawn->IsSkydiving() && !bot->Pawn->IsParachuteOpen())
			{
				if (bot->Pawn->CharacterMovement)
					bot->Pawn->CharacterMovement->SetMovementMode(EMovementMode::MOVE_Falling, 0);
				bot->Pawn->BeginSkydiving(true);
			}
			return;
		}

		TickGround(bot);
	}

	inline void SweepDead()
	{
		for (int i = static_cast<int>(All.size()) - 1; i >= 0; --i)
		{
			auto* bot = All[static_cast<size_t>(i)];
			if (bot && bot->Pawn && !bot->Pawn->IsDead())
				continue;
			delete bot;
			All.erase(All.begin() + static_cast<size_t>(i));
		}
	}

	inline void TickAll()
	{
		if (!kSolo)
			return;
		WatchWorld();
		auto* state = State();
		if (!state || state->GamePhase <= EAthenaGamePhase::Setup)
			return;

		EnsureManager();
		Fill();
		SweepDead();
		for (auto* bot : All)
			TickOne(bot);
		SyncPlayersLeft();
	}

	inline void OnAircraftExitedDropZone(AFortGameModeAthena* mode, AFortAthenaAircraft* aircraft)
	{
		if (kSolo)
		{
			for (auto* bot : All)
			{
				if (bot && !bot->Jumped)
					JumpFromBus(bot);
			}
		}
		AircraftExitOriginal(mode, aircraft);
	}

	inline __int64 OnAircraftEnteredDropZone(AFortGameModeAthena* mode, AFortAthenaAircraft* aircraft)
	{
		const auto result = AircraftEnterOriginal(mode, aircraft);
		if (kSolo)
		{
			if (auto* state = State())
				state->GamePhaseStep = EAthenaGamePhaseStep::BusFlying;
			if (!LoggedBus)
			{
				LoggedBus = true;
				Note("bus entered drop zone");
			}
		}
		return result;
	}

	inline void Bind()
	{
		if (!kSolo || Hooked)
			return;
		Patch::Attach(reinterpret_cast<void**>(&SpawnBotOriginal), reinterpret_cast<void*>(&OnSpawnBot));
		Patch::Attach(reinterpret_cast<void**>(&AircraftExitOriginal), reinterpret_cast<void*>(&OnAircraftExitedDropZone));
		Patch::Attach(reinterpret_cast<void**>(&AircraftEnterOriginal), reinterpret_cast<void*>(&OnAircraftEnteredDropZone));
		Hooked = true;
		Note("solo bots hooked (phoebe + bus)");
	}
}
