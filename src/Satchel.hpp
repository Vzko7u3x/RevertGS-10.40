#pragma once

#include "Pch.hpp"

#include <initializer_list>

namespace Revert::Satchel
{
	inline void Push(AFortPlayerController* pc)
	{
		if (!pc || !pc->WorldInventory)
			return;
		pc->HandleWorldInventoryLocalUpdate();
		pc->WorldInventory->HandleInventoryLocalUpdate();
		pc->WorldInventory->bRequiresLocalUpdate = true;
		pc->WorldInventory->Inventory.MarkArrayDirty();
		pc->WorldInventory->ForceNetUpdate();
		pc->ClientForceUpdateQuickbar(EFortQuickBars::Primary);
		pc->ClientForceUpdateQuickbar(EFortQuickBars::Secondary);
		pc->ClientForceWorldInventoryUpdate();
	}

	inline UFortWorldItem* Locate(AFortPlayerController* pc, const FGuid& guid)
	{
		if (!pc || !pc->WorldInventory)
			return nullptr;
		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		for (int i = 0; i < bag.Num(); ++i)
		{
			if (bag[i] && bag[i]->ItemEntry.ItemGuid == guid)
				return bag[i];
		}
		return nullptr;
	}

	inline UFortWorldItem* Locate(AFortPlayerController* pc, UFortItemDefinition* def)
	{
		if (!pc || !pc->WorldInventory || !def)
			return nullptr;
		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		for (int i = 0; i < bag.Num(); ++i)
		{
			if (bag[i] && bag[i]->ItemEntry.ItemDefinition == def)
				return bag[i];
		}
		return nullptr;
	}

	inline FFortItemEntry* Replica(AFortPlayerController* pc, const FGuid& guid)
	{
		if (!pc || !pc->WorldInventory)
			return nullptr;
		auto& rows = pc->WorldInventory->Inventory.ReplicatedEntries;
		for (int i = 0; i < rows.Num(); ++i)
		{
			if (rows[i].ItemGuid == guid)
				return rows.GetRef(i);
		}
		return nullptr;
	}

	inline FFortItemEntry* Replica(AFortPlayerController* pc, UFortItemDefinition* def)
	{
		if (!pc || !pc->WorldInventory || !def)
			return nullptr;
		auto& rows = pc->WorldInventory->Inventory.ReplicatedEntries;
		for (int i = 0; i < rows.Num(); ++i)
		{
			if (rows[i].ItemDefinition == def)
				return rows.GetRef(i);
		}
		return nullptr;
	}

	inline int CountOf(AFortPlayerController* pc, UFortItemDefinition* def)
	{
		if (auto* row = Replica(pc, def))
			return row->Count;
		if (auto* item = Locate(pc, def))
			return item->ItemEntry.Count;
		return 0;
	}

	inline int CapOf(AFortPlayerController* pc, UFortItemDefinition* def)
	{
		if (!def)
			return 1;
		if (auto* resource = As<UFortResourceItemDefinition>(def))
		{
			if (pc)
			{
				const int cap = UFortKismetLibrary::GetResourceMaxStackSize(pc, resource->ResourceType);
				if (cap > 0)
					return cap;
			}
			if (def->MaxStackSize > 0)
				return def->MaxStackSize;
			return 999;
		}
		if (def->MaxStackSize > 0)
			return def->MaxStackSize;
		if (def->IsA(UFortWorldItemDefinition::StaticClass()))
			return 999;
		return 1;
	}

	inline void HideToast(FFortItemEntry* row)
	{
		if (!row)
			return;
		for (int i = 0; i < row->StateValues.Num(); ++i)
		{
			if (row->StateValues[i].StateType == EFortItemEntryState::ShouldShowItemToast)
			{
				row->StateValues[i].IntValue = 0;
				row->StateValues[i].NameValue = FName{};
			}
		}
	}

	inline int ClipOf(UFortItemDefinition* def)
	{
		auto* weapon = As<UFortWeaponItemDefinition>(def);
		if (!weapon || !weapon->WeaponStatHandle.DataTable)
			return 0;
		auto* row = reinterpret_cast<FFortRangedWeaponStats*>(weapon->WeaponStatHandle.DataTable->RowMap[weapon->WeaponStatHandle.RowName]);
		return row ? row->ClipSize : 30;
	}

	inline UFortWorldItem* Grant(AFortPlayerController* pc, UFortItemDefinition* def, int count, int ammo = -1)
	{
		if (!pc || !pc->WorldInventory || !def || count <= 0)
			return nullptr;

		if (ammo < 0)
			ammo = ClipOf(def);

		const int cap = CapOf(pc, def);
		auto* stacked = Locate(pc, def);
		auto* row = stacked ? Replica(pc, stacked->ItemEntry.ItemGuid) : Replica(pc, def);
		if (stacked || row)
		{
			const int have = row ? row->Count : stacked->ItemEntry.Count;
			if (!def->bAllowMultipleStacks || have < cap)
			{
				const int room = cap - have;
				const int add = room > 0 ? (count < room ? count : room) : 0;
				if (add > 0)
				{
					const int next = have + add;
					if (stacked)
						stacked->ItemEntry.Count = next;
					if (row)
					{
						row->Count = next;
						HideToast(row);
						pc->WorldInventory->Inventory.MarkItemDirty(*row);
					}
					else if (stacked)
					{
						stacked->ItemEntry.Count = next;
						stacked->ItemEntry.bIsReplicatedCopy = true;
						HideToast(&stacked->ItemEntry);
						auto& added = pc->WorldInventory->Inventory.ReplicatedEntries.Add(stacked->ItemEntry);
						added.Count = next;
						added.bIsReplicatedCopy = true;
						HideToast(&added);
						pc->WorldInventory->Inventory.MarkItemDirty(added);
					}
				}
				count -= add;
				if (count <= 0 || !def->bAllowMultipleStacks)
					return stacked;
			}
		}

		auto* instance = As<UFortWorldItem>(def->CreateTemporaryItemInstanceBP(count, 1));
		if (!instance)
			return nullptr;

		instance->ItemEntry.Count = count;
		instance->ItemEntry.LoadedAmmo = ammo;
		instance->SetOwningControllerForTemporaryItem(pc);
		pc->WorldInventory->Inventory.ItemInstances.Add(instance);
		auto& fresh = pc->WorldInventory->Inventory.ReplicatedEntries.Add(instance->ItemEntry);
		fresh.LoadedAmmo = ammo;
		fresh.bIsReplicatedCopy = true;
		HideToast(&fresh);
		pc->WorldInventory->Inventory.MarkItemDirty(fresh);
		return instance;
	}

	inline void Strip(AFortPlayerController* pc, const FGuid& guid, int count)
	{
		auto* instance = Locate(pc, guid);
		auto* row = Replica(pc, guid);
		if (!instance && !row)
			return;

		const int have = row ? row->Count : instance->ItemEntry.Count;
		const int next = have - count;
		if (next > 0)
		{
			if (instance)
				instance->ItemEntry.Count = next;
			if (row)
			{
				row->Count = next;
				pc->WorldInventory->Inventory.MarkItemDirty(*row);
			}
			return;
		}

		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		auto& rows = pc->WorldInventory->Inventory.ReplicatedEntries;
		for (int i = 0; i < bag.Num(); ++i)
		{
			if (bag[i] && bag[i]->ItemEntry.ItemGuid == guid)
			{
				bag.Remove(i);
				break;
			}
		}
		for (int i = 0; i < rows.Num(); ++i)
		{
			if (rows[i].ItemGuid == guid)
			{
				rows.Remove(i);
				break;
			}
		}
	}

	inline bool KeepDef(UFortItemDefinition* def)
	{
		if (!def)
			return false;
		if (def->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
			return true;
		if (def->IsA(UFortBuildingItemDefinition::StaticClass()))
			return true;

		const auto name = def->GetName();
		return name.find("Harvest") != std::string::npos
			|| name.find("Pickaxe") != std::string::npos
			|| name.find("BuildingItemData") != std::string::npos;
	}

	inline void EmptyDroppables(AFortPlayerController* pc)
	{
		if (!pc || !pc->WorldInventory)
			return;

		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		auto& rows = pc->WorldInventory->Inventory.ReplicatedEntries;

		for (int i = bag.Num() - 1; i >= 0; --i)
		{
			if (!bag[i] || KeepDef(bag[i]->ItemEntry.ItemDefinition) || !bag[i]->CanBeDropped())
				continue;
			bag.Remove(i);
		}
		for (int i = rows.Num() - 1; i >= 0; --i)
		{
			if (KeepDef(rows[i].ItemDefinition))
				continue;
			if (auto* world = As<UFortWorldItemDefinition>(rows[i].ItemDefinition); world && world->bCanBeDropped)
				rows.Remove(i);
		}
		Push(pc);
	}

	inline AFortPickupAthena* Toss(UFortItemDefinition* def, int count, FVector where, int ammo = 0, int tossMax = 0)
	{
		if (!def || !World())
			return nullptr;
		if (tossMax <= 0)
			tossMax = def->MaxStackSize > 0 ? def->MaxStackSize : 999;

		auto* pickup = World()->SpawnActor<AFortPickupAthena>(where);
		if (!pickup)
			return nullptr;

		pickup->PrimaryPickupItemEntry.ItemDefinition = def;
		pickup->PrimaryPickupItemEntry.Count = count;
		pickup->PrimaryPickupItemEntry.LoadedAmmo = ammo;
		pickup->OnRep_PrimaryPickupItemEntry();
		pickup->TossPickup(where, nullptr, tossMax, true, EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::Unset);
		return pickup;
	}

	inline AFortPickupAthena* TossFromContainer(UFortItemDefinition* def, int count, FVector where, int ammo = 0)
	{
		if (!def || !World())
			return nullptr;
		const int tossMax = def->MaxStackSize > 0 ? def->MaxStackSize : 999;

		auto* pickup = World()->SpawnActor<AFortPickupAthena>(where);
		if (!pickup)
			return nullptr;

		pickup->PrimaryPickupItemEntry.ItemDefinition = def;
		pickup->PrimaryPickupItemEntry.Count = count;
		pickup->PrimaryPickupItemEntry.LoadedAmmo = ammo;
		pickup->OnRep_PrimaryPickupItemEntry();
		pickup->bTossedFromContainer = true;
		pickup->OnRep_TossedFromContainer();
		pickup->TossPickup(where, nullptr, tossMax, true, EFortPickupSourceTypeFlag::Container, EFortPickupSpawnSource::Chest);
		return pickup;
	}

	inline void AddResource(AFortPlayerController* pc, UFortItemDefinition* def, int amount)
	{
		if (!pc || !pc->WorldInventory || !def || amount <= 0)
			return;

		const int cap = CapOf(pc, def);
		auto* row = Replica(pc, def);
		auto* instance = Locate(pc, def);
		if (!row && !instance)
		{
			const int give = amount > cap ? cap : amount;
			Grant(pc, def, give);
			Push(pc);
			const int leftover = amount - give;
			if (leftover > 0 && pc->MyFortPawn)
				Toss(def, leftover, pc->MyFortPawn->K2_GetActorLocation(), 0, cap);
			return;
		}

		const int have = row ? row->Count : instance->ItemEntry.Count;
		int leftover = 0;
		int next = have + amount;
		if (next > cap)
		{
			leftover = next - cap;
			next = cap;
		}

		if (instance)
			instance->ItemEntry.Count = next;
		if (row)
		{
			row->Count = next;
			HideToast(row);
			pc->WorldInventory->Inventory.MarkItemDirty(*row);
		}
		else
		{
			instance->ItemEntry.Count = next;
			instance->ItemEntry.bIsReplicatedCopy = true;
			HideToast(&instance->ItemEntry);
			auto& added = pc->WorldInventory->Inventory.ReplicatedEntries.Add(instance->ItemEntry);
			added.Count = next;
			added.bIsReplicatedCopy = true;
			HideToast(&added);
			pc->WorldInventory->Inventory.MarkItemDirty(added);
		}
		Push(pc);
		if (leftover > 0 && pc->MyFortPawn)
			Toss(def, leftover, pc->MyFortPawn->K2_GetActorLocation(), 0, cap);
	}

	inline UFortItemDefinition* ResourceFor(EFortResourceType type)
	{
		switch (type)
		{
		case EFortResourceType::Wood:  return Find<UFortItemDefinition>("/Game/Items/ResourcePickups/WoodItemData.WoodItemData");
		case EFortResourceType::Stone: return Find<UFortItemDefinition>("/Game/Items/ResourcePickups/StoneItemData.StoneItemData");
		case EFortResourceType::Metal: return Find<UFortItemDefinition>("/Game/Items/ResourcePickups/MetalItemData.MetalItemData");
		default: return nullptr;
		}
	}

	inline bool HasHarvest(AFortPlayerController* pc)
	{
		if (!pc || !pc->WorldInventory)
			return false;
		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		for (int i = 0; i < bag.Num(); ++i)
		{
			auto* def = bag[i] ? bag[i]->ItemEntry.ItemDefinition : nullptr;
			if (!def)
				continue;
			if (def->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
				return true;
			const auto name = def->GetName();
			if (name.find("Harvest") != std::string::npos || name.find("Pickaxe") != std::string::npos)
				return true;
		}
		return false;
	}

	inline UFortWeaponMeleeItemDefinition* ScanHarvest()
	{
		if (!UObject::GObjects)
			return nullptr;

		UFortWeaponMeleeItemDefinition* melee = nullptr;
		for (int i = 0; i < UObject::GObjects->Num(); ++i)
		{
			auto* object = UObject::GObjects->GetObjectById(i);
			if (!object)
				continue;

			if (auto* cosmetic = As<UAthenaPickaxeItemDefinition>(object))
			{
				if (cosmetic->GetName().find("Default__") != std::string::npos)
					continue;
				if (cosmetic->WeaponDefinition)
					return cosmetic->WeaponDefinition;
			}

			if (!melee)
			{
				if (auto* def = As<UFortWeaponMeleeItemDefinition>(object))
				{
					const auto name = def->GetName();
					if (name.find("Default__") != std::string::npos)
						continue;
					if (name.find("Harvest") != std::string::npos || name.find("Pickaxe") != std::string::npos)
						melee = def;
				}
			}
		}
		return melee;
	}

	inline UAthenaPickaxeItemDefinition* BindPickaxe(AFortPlayerController* pc)
	{
		if (pc && pc->CosmeticLoadoutPC.Pickaxe && pc->CosmeticLoadoutPC.Pickaxe->WeaponDefinition)
			return pc->CosmeticLoadoutPC.Pickaxe;

		if (auto* cosmetic = Find<UAthenaPickaxeItemDefinition>("/Game/Athena/Items/Cosmetics/Pickaxes/DefaultPickaxe.DefaultPickaxe"))
		{
			if (cosmetic->WeaponDefinition)
			{
				if (pc)
					pc->CosmeticLoadoutPC.Pickaxe = cosmetic;
				return cosmetic;
			}
		}

		if (!UObject::GObjects)
			return nullptr;

		for (int i = 0; i < UObject::GObjects->Num(); ++i)
		{
			auto* object = UObject::GObjects->GetObjectById(i);
			auto* cosmetic = object ? As<UAthenaPickaxeItemDefinition>(object) : nullptr;
			if (!cosmetic || !cosmetic->WeaponDefinition)
				continue;
			if (cosmetic->GetName().find("Default__") != std::string::npos)
				continue;
			if (pc)
				pc->CosmeticLoadoutPC.Pickaxe = cosmetic;
			return cosmetic;
		}
		return nullptr;
	}

	inline UFortWeaponMeleeItemDefinition* HarvestDef(AFortPlayerController* pc)
	{
		if (auto* cosmetic = BindPickaxe(pc); cosmetic && cosmetic->WeaponDefinition)
			return cosmetic->WeaponDefinition;

		if (auto* harvest = Find<UFortWeaponMeleeItemDefinition>("/Game/Athena/Items/Weapons/WID_Harvest_Pickaxe_Athena_C_T01.WID_Harvest_Pickaxe_Athena_C_T01"))
			return harvest;

		return ScanHarvest();
	}

	inline void GiveHarvest(AFortPlayerController* pc)
	{
		if (HasHarvest(pc))
			return;

		if (auto* harvest = HarvestDef(pc))
		{
			Grant(pc, harvest, 1);
			pc->AddItemToQuickBars(harvest, EFortQuickBars::Primary, 0);
			static bool logged = false;
			if (!logged)
			{
				logged = true;
				Note("harvest {}", harvest->GetName());
			}
			return;
		}

		static bool missing = false;
		if (!missing)
		{
			missing = true;
			Note("harvest missing");
		}
	}

	inline void EquipHarvest(AFortPlayerController* pc, AFortPawn* pawn)
	{
		if (!pc || !pawn)
			return;
		auto* harvest = HarvestDef(pc);
		if (!harvest)
			return;
		if (auto* stack = Locate(pc, harvest))
			pawn->EquipWeaponDefinition(harvest, stack->ItemEntry.ItemGuid);
	}

	inline UFortItemDefinition* FindDef(const char* leaf)
	{
		if (!leaf || !*leaf || !UObject::GObjects)
			return nullptr;

		static std::unordered_map<std::string, UFortItemDefinition*> cache;
		if (auto it = cache.find(leaf); it != cache.end())
			return it->second;

		auto* type = UFortItemDefinition::StaticClass();
		UFortItemDefinition* fuzzy = nullptr;
		for (int i = 0; i < UObject::GObjects->Num(); ++i)
		{
			auto* object = UObject::GObjects->GetObjectById(i);
			if (!object || (type && !object->IsA(type)))
				continue;
			const auto name = object->GetName();
			if (name.find("Default__") != std::string::npos)
				continue;
			if (name == leaf)
			{
				cache[leaf] = static_cast<UFortItemDefinition*>(object);
				return cache[leaf];
			}
			if (!fuzzy && name.find(leaf) != std::string::npos)
				fuzzy = static_cast<UFortItemDefinition*>(object);
		}
		if (fuzzy)
			cache[leaf] = fuzzy;
		return fuzzy;
	}

	inline UFortItemDefinition* PickOne(std::initializer_list<const char*> names)
	{
		UFortItemDefinition* found[8]{};
		int n = 0;
		for (auto* name : names)
		{
			if (n >= 8)
				break;
			if (auto* def = FindDef(name))
				found[n++] = def;
		}
		if (!n)
			return nullptr;
		return found[RandRange(0, n - 1)];
	}

	inline void GiveAmmo(AFortPlayerController* pc)
	{
		Grant(pc, FindDef("AthenaAmmoDataShells"), 999);
		Grant(pc, FindDef("AthenaAmmoDataBulletsMedium"), 999);
		Grant(pc, FindDef("AthenaAmmoDataBulletsLight"), 999);
		Grant(pc, FindDef("AthenaAmmoDataBulletsHeavy"), 999);
		Grant(pc, Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataShells.AthenaAmmoDataShells"), 999);
		Grant(pc, Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsMedium.AthenaAmmoDataBulletsMedium"), 999);
		Grant(pc, Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsLight.AthenaAmmoDataBulletsLight"), 999);
		Grant(pc, Find<UFortItemDefinition>("/Game/Athena/Items/Ammo/AthenaAmmoDataBulletsHeavy.AthenaAmmoDataBulletsHeavy"), 999);
	}

	inline bool HasLateKit(AFortPlayerController* pc)
	{
		if (!pc || !pc->WorldInventory)
			return false;
		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		for (int i = 0; i < bag.Num(); ++i)
		{
			auto* def = bag[i] ? bag[i]->ItemEntry.ItemDefinition : nullptr;
			if (!def || def->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) || KeepDef(def))
				continue;
			if (def->IsA(UFortWeaponItemDefinition::StaticClass()) || def->IsA(UFortWorldItemDefinition::StaticClass()))
			{
				const auto name = def->GetName();
				if (name.find("WID_") != std::string::npos || name.find("Athena_") != std::string::npos)
					return true;
			}
		}
		return false;
	}

	inline void EquipLoadout(AFortPlayerController* pc, AFortPawn* pawn)
	{
		if (!pc || !pawn || !pc->WorldInventory)
			return;
		auto& bag = pc->WorldInventory->Inventory.ItemInstances;
		for (int i = 0; i < bag.Num(); ++i)
		{
			auto* def = bag[i] ? As<UFortWeaponItemDefinition>(bag[i]->ItemEntry.ItemDefinition) : nullptr;
			if (!def || def->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) || def->IsA(UFortBuildingItemDefinition::StaticClass()))
				continue;
			pawn->EquipWeaponDefinition(def, bag[i]->ItemEntry.ItemGuid);
			return;
		}
		EquipHarvest(pc, pawn);
	}

	inline void GiveListed(AFortPlayerController* pc, TArray<FItemAndCount>& rows)
	{
		for (int i = 0; i < rows.Num(); ++i)
		{
			if (!rows[i].Item || rows[i].Count <= 0)
				continue;
			if (Locate(pc, rows[i].Item))
				continue;
			Grant(pc, rows[i].Item, rows[i].Count);
		}
	}

	inline void GiveBuildKit(AFortPlayerController* pc)
	{
		if (!pc)
			return;

		if (auto* mode = Mode())
			GiveListed(pc, mode->StartingItems);

		if (auto* state = State(); state && state->CurrentPlaylistInfo.BasePlaylist)
			GiveListed(pc, state->CurrentPlaylistInfo.BasePlaylist->InventoryItemsToGrant);

		static const char* pieces[] = {
			"/Game/Items/Weapons/BuildingTools/BuildingItemData_Wall.BuildingItemData_Wall",
			"/Game/Items/Weapons/BuildingTools/BuildingItemData_Floor.BuildingItemData_Floor",
			"/Game/Items/Weapons/BuildingTools/BuildingItemData_Stair_W.BuildingItemData_Stair_W",
			"/Game/Items/Weapons/BuildingTools/BuildingItemData_RoofS.BuildingItemData_RoofS",
			"/Game/Items/Weapons/BuildingTools/EditTool.EditTool",
		};
		for (int i = 0; i < 5; ++i)
		{
			auto* def = Find<UFortItemDefinition>(pieces[i]);
			if (!def)
				continue;
			if (!Locate(pc, def))
				Grant(pc, def, 1);
			if (i < 4)
				pc->AddItemToQuickBars(def, EFortQuickBars::Secondary, i);
		}
	}

	inline void SeedMats(AFortPlayerController* pc, int count)
	{
		if (!pc || count <= 0)
			return;
		Grant(pc, Find<UFortItemDefinition>("/Game/Items/ResourcePickups/WoodItemData.WoodItemData"), count);
		Grant(pc, Find<UFortItemDefinition>("/Game/Items/ResourcePickups/StoneItemData.StoneItemData"), count);
		Grant(pc, Find<UFortItemDefinition>("/Game/Items/ResourcePickups/MetalItemData.MetalItemData"), count);
	}

	inline void GiveStarter(AFortPlayerController* pc)
	{
		if (!pc)
			return;
		GiveHarvest(pc);
		GiveBuildKit(pc);
		SeedMats(pc, kPlotMode ? 500 : 1);
		Push(pc);
	}

	inline void LateKit(AFortPlayerController* pc)
	{
		if (HasLateKit(pc))
		{
			GiveHarvest(pc);
			GiveBuildKit(pc);
			return;
		}

		GiveHarvest(pc);
		GiveBuildKit(pc);
		SeedMats(pc, 500);

		auto* rifle = PickOne({
			"WID_Assault_Auto_Athena_R_Ore_T03",
			"WID_Assault_AutoHigh_Athena_SR_Ore_T03",
		});
		auto* shotgun = PickOne({
			"WID_Shotgun_Standard_Athena_SR_Ore_T03",
			"WID_Shotgun_HighSemiAuto_Athena_VR_Ore_T03",
			"ID_Shotgun_HighSemiAuto_Athena_VR_Ore_T03",
		});
		auto* sniper = PickOne({
			"WID_Sniper_BoltAction_Scope_Athena_SR_Ore_T03",
			"WID_Sniper_Heavy_Athena_VR_Ore_T03",
		});
		auto* smg = PickOne({
			"WID_Pistol_Scavenger_Athena_VR_Ore_T03",
		});
		auto* move = PickOne({
			"Athena_ShockGrenade",
			"Athena_GrapplingHook",
			"WID_Hook_Gun_Slide",
			"WID_Hook_Gun_Athena_SR_Ore_T03",
			"WID_Hook_Gun_Athena",
			"Athena_Rift_Item",
		});
		auto* heal = PickOne({
			"WID_Athena_Flopper",
			"WID_Athena_Flopper_Effective",
			"Athena_ShieldSmall",
			"Athena_PurpleStuff",
			"Athena_Medkit",
			"Athena_Shields",
		});

		UFortItemDefinition* slots[6] = { rifle, shotgun, sniper, smg, move, heal };
		const int counts[6] = { 1, 1, 1, 1, 6, 3 };
		for (int i = 0; i < 6; ++i)
		{
			if (!slots[i])
				continue;
			Grant(pc, slots[i], counts[i]);
			pc->AddItemToQuickBars(slots[i], EFortQuickBars::Primary, i + 1);
		}

		GiveAmmo(pc);
		Push(pc);
		Note("late kit {} / {} / {} / {} / {} / {}",
			rifle ? rifle->GetName() : "?",
			shotgun ? shotgun->GetName() : "?",
			sniper ? sniper->GetName() : "?",
			smg ? smg->GetName() : "?",
			move ? move->GetName() : "?",
			heal ? heal->GetName() : "?");
	}
}
