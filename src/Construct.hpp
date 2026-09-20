#pragma once

#include "Pch.hpp"
#include "Satchel.hpp"

#include <cmath>

namespace Revert::Construct
{
	inline UClass* ChosenClass(AFortPlayerControllerAthena* pc, const FCreateBuildingActorData& data)
	{
		if (pc && pc->BroadcastRemoteClientInfo && pc->BroadcastRemoteClientInfo->RemoteBuildableClass)
			return pc->BroadcastRemoteClientInfo->RemoteBuildableClass;
		return data.BuildingClassData.BuildingClass;
	}

	inline bool AllowedClass(UClass* klass)
	{
		auto* state = State();
		if (!state || !state->BuildingActorClasses.Num())
			return true;
		for (int i = 0; i < state->BuildingActorClasses.Num(); ++i)
		{
			if (state->BuildingActorClasses[i] == klass)
				return true;
		}
		return false;
	}

	inline bool RememberClass(AFortBroadcastRemoteClientInfo* info, UClass* klass)
	{
		if (!info || !klass)
			return true;
		info->RemoteBuildableClass = klass;
		info->OnRep_RemoteBuildableClass();
		return true;
	}

	inline bool Place(AFortPlayerControllerAthena* pc, FCreateBuildingActorData data)
	{
		auto* klass = ChosenClass(pc, data);
		auto* pawn = pc ? As<AFortPlayerPawnAthena>(pc->Pawn) : nullptr;
		auto* ps = pc ? As<AFortPlayerStateAthena>(pc->PlayerState) : nullptr;
		if (!pc || !klass || !pawn || !ps || !World() || !AllowedClass(klass))
			return true;

		using ProbeFn = __int64 (*)(UObject*, UObject*, FVector, FRotator, char, TArray<ABuildingSMActor*>*, char*);
		static auto Probe = Rva::Rel<ProbeFn>(Rva::CantBuild);

		TArray<ABuildingSMActor*> overlap;
		char extra = 0;
		if (Probe(World(), klass, data.BuildLoc, data.BuildRot, data.bMirrored, &overlap, &extra))
		{
			overlap.Free();
			return true;
		}

		for (int i = 0; i < overlap.Num(); ++i)
		{
			if (overlap[i])
				overlap[i]->K2_DestroyActor();
		}
		overlap.Free();

		auto* piece = World()->SpawnActor<ABuildingSMActor>(data.BuildLoc, data.BuildRot, klass);
		if (!piece)
			return true;

		if (!pc->bBuildFree)
		{
			UFortItemDefinition* mat = UFortKismetLibrary::K2_GetResourceItemDefinition(piece->ResourceType);
			if (!mat)
				mat = Satchel::ResourceFor(piece->ResourceType);
			if (!mat || Satchel::CountOf(pc, mat) < 10)
			{
				piece->K2_DestroyActor();
				return true;
			}
			FGuid guid{};
			if (auto* stack = Satchel::Locate(pc, mat))
				guid = stack->ItemEntry.ItemGuid;
			else if (auto* row = Satchel::Replica(pc, mat))
				guid = row->ItemGuid;
			Satchel::Strip(pc, guid, 10);
			Satchel::Push(pc);
		}

		piece->bPlayerPlaced = true;
		piece->TeamIndex = ps->TeamIndex;
		piece->InitializeKismetSpawnedBuildingActor(piece, pc, true);
		return true;
	}

	inline UFortWeaponItemDefinition* EditToolDef()
	{
		return Find<UFortWeaponItemDefinition>("/Game/Items/Weapons/BuildingTools/EditTool.EditTool");
	}

	inline AFortWeap_EditingTool* HeldEditTool(AFortPlayerController* pc)
	{
		auto* pawn = pc ? As<AFortPawn>(pc->MyFortPawn ? pc->MyFortPawn : pc->Pawn) : nullptr;
		return pawn ? As<AFortWeap_EditingTool>(pawn->CurrentWeapon) : nullptr;
	}

	inline bool BeginEdit(AFortPlayerController* pc, ABuildingSMActor* piece)
	{
		if (!pc || !piece || !piece->bPlayerPlaced)
			return true;

		auto* pawn = As<AFortPawn>(pc->MyFortPawn ? pc->MyFortPawn : pc->Pawn);
		auto* def = EditToolDef();
		if (!pawn || !def)
			return true;

		if (!Satchel::Locate(pc, def))
		{
			Satchel::Grant(pc, def, 1);
			Satchel::Push(pc);
		}

		FGuid guid{};
		if (auto* item = Satchel::Locate(pc, def))
			guid = item->ItemEntry.ItemGuid;

		auto* tool = As<AFortWeap_EditingTool>(pawn->EquipWeaponDefinition(def, guid));
		if (!tool)
			return true;

		piece->EditingPlayer = As<AFortPlayerStateZone>(pc->PlayerState);
		piece->OnRep_EditingPlayer();
		tool->EditActor = piece;
		tool->OnRep_EditActor();
		return true;
	}

	inline bool EndEdit(AFortPlayerController* pc, ABuildingSMActor* piece)
	{
		if (piece)
		{
			piece->EditingPlayer = nullptr;
			piece->OnRep_EditingPlayer();
		}
		if (auto* tool = HeldEditTool(pc))
		{
			tool->EditActor = nullptr;
			tool->OnRep_EditActor();
		}
		return true;
	}

	inline bool Edit(AFortPlayerController* pc, ABuildingSMActor* piece, UClass* next, uint8_t turns, bool mirrored)
	{
		if (!pc || !piece || !next || piece->bDestroyed)
			return true;

		piece->EditingPlayer = nullptr;

		using ReplaceFn = ABuildingSMActor* (*)(ABuildingSMActor*, __int64, UClass*, int, int, uint8_t, AFortPlayerController*);
		static auto Replace = Rva::Rel<ReplaceFn>(Rva::ReplaceBuilding);
		auto* fresh = Replace(piece, 1, next, piece->CurrentBuildingLevel, static_cast<int>(turns), mirrored ? 1 : 0, pc);
		if (!fresh)
			return true;

		fresh->bPlayerPlaced = true;
		if (auto* ps = As<AFortPlayerStateAthena>(pc->PlayerState))
		{
			fresh->SetTeam(ps->TeamIndex);
			fresh->OnRep_Team();
		}
		if (auto* tool = HeldEditTool(pc))
		{
			tool->EditActor = nullptr;
			tool->OnRep_EditActor();
		}
		return true;
	}

	inline bool Repair(AFortPlayerController* pc, ABuildingSMActor* piece)
	{
		if (!pc || !piece)
			return true;
		piece->SetHealth(piece->GetMaxHealth());
		return true;
	}

	inline char (*DamageOriginal)(ABuildingActor*, float, FGameplayTagContainer, __int64, AController*, AActor*) = Rva::Rel<decltype(DamageOriginal)>(Rva::BuildingDamage);

	inline int HarvestCount(ABuildingSMActor* building, float damage)
	{
		if (!building || damage <= 0.f)
			return 0;

		auto* table = Find<UCurveTable>("/Game/Athena/Balance/DataTables/AthenaResourceRates.AthenaResourceRates");
		if (!table)
			table = Find<UCurveTable>("/Game/Balance/DataTables/ResourceRates.ResourceRates");

		float total = 0.f;
		TEnumAsByte<EEvaluateCurveTableResult> result{};
		if (table && building->BuildingResourceAmountOverride.RowName.ComparisonIndex > 0)
			UDataTableFunctionLibrary::EvaluateCurveTableRow(table, building->BuildingResourceAmountOverride.RowName, 0.f, FString(L"Harvest"), &result, &total);

		float maxHp = building->GetMaxHealth();
		if (maxHp < 1.f)
			maxHp = 1.f;
		int amount = 0;
		if (total > 0.f)
			amount = static_cast<int>(std::lround(total * damage / maxHp));
		if (amount <= 0 && total > 0.f)
			amount = 1;
		if (amount <= 0)
			amount = damage >= 100.f ? 6 : 3;
		if (building->MaxResourcesToSpawn > 0 && amount > building->MaxResourcesToSpawn)
			amount = building->MaxResourcesToSpawn;
		return amount;
	}

	inline void PayHarvest(ABuildingActor* actor, AController* instigator, AActor* causer, float damage)
	{
		if (!actor || !instigator || !causer)
			return;

		auto* building = As<ABuildingSMActor>(actor);
		auto* pc = As<AFortPlayerController>(instigator);
		auto* weapon = As<AFortWeapon>(causer);
		if (!building || building->bPlayerPlaced || !pc || !pc->WorldInventory || !weapon || !weapon->WeaponData)
			return;
		if (pc->PlayerState && pc->PlayerState->bIsABot)
			return;
		if (!weapon->WeaponData->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
			return;
		if (building->GetHealth() == 1.f)
			return;

		UFortItemDefinition* def = UFortKismetLibrary::K2_GetResourceItemDefinition(building->ResourceType);
		if (!def)
			def = Satchel::ResourceFor(building->ResourceType);
		if (!def)
			return;

		const int amount = HarvestCount(building, damage);
		if (amount <= 0)
			return;

		Satchel::AddResource(pc, def, amount);

		const bool broken = building->GetHealth() - damage <= 0.f;
		const bool weak = damage >= 100.f;
		pc->ClientReportDamagedResourceBuilding(building, building->ResourceType, amount, broken, weak);
		building->ForceNetUpdate();
	}

	inline char OnBuildingDamage(ABuildingActor* building, float damage, FGameplayTagContainer tags, __int64 effect, AController* instigator, AActor* causer)
	{
		PayHarvest(building, instigator, causer, damage);
		return DamageOriginal(building, damage, tags, effect, instigator, causer);
	}
}
