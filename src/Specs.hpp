#pragma once

#include "Pch.hpp"
#include "Patch.hpp"

namespace Revert::Specs
{
	using GiveFn = FGameplayAbilitySpecHandle* (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle*, FGameplayAbilitySpec);
	using CtorFn = void (*)(FGameplayAbilitySpec*, UGameplayAbility*, int, int, UObject*);
	using ClearFn = void (*)(UAbilitySystemComponent*, const FGameplayAbilitySpecHandle&);
	using FireFn = bool (*)(UAbilitySystemComponent*, FGameplayAbilitySpecHandle, FPredictionKey, UGameplayAbility**, void*, const FGameplayEventData*);

	inline GiveFn Give = Rva::Rel<GiveFn>(Rva::GiveAbility);
	inline GiveFn GiveOnce = Rva::Rel<GiveFn>(Rva::GiveAbilityOnce);
	inline CtorFn MakeSpec = Rva::Rel<CtorFn>(Rva::AbilitySpecCtor);
	inline ClearFn Drop = Rva::Rel<ClearFn>(Rva::ClearAbility);
	inline FireFn Fire = Rva::Rel<FireFn>(Rva::TryActivate);

	inline void ClearMontageAbilities(UAbilitySystemComponent* asc)
	{
		if (!asc)
			return;

		std::vector<FGameplayAbilitySpecHandle> kill;
		for (int i = 0; i < asc->ActivatableAbilities.Items.Num(); ++i)
		{
			auto& spec = asc->ActivatableAbilities.Items[i];
			if (spec.SourceObject && As<UFortMontageItemDefinitionBase>(spec.SourceObject))
				kill.push_back(spec.Handle);
		}
		for (auto& handle : kill)
			Drop(asc, handle);
	}

	inline void ActivateOnce(UAbilitySystemComponent* asc, UGameplayAbility* ability, UObject* source)
	{
		if (!asc || !ability)
			return;

		const auto name = ability->GetName();
		if (name.find("ClientPilot") != std::string::npos)
		{
			Note("emote skipped bad ability {}", name);
			return;
		}

		ClearMontageAbilities(asc);

		FGameplayAbilitySpec spec{};
		MakeSpec(&spec, ability, 1, -1, source);
		spec.RemoveAfterActivation = 1;
		GiveOnce(asc, &spec.Handle, spec);
	}

	inline void AttachSet(UAbilitySystemComponent* asc, UFortAbilitySet* set)
	{
		if (!asc || !set)
			return;

		for (int i = 0; i < set->GameplayAbilities.Num(); ++i)
		{
			auto* klass = set->GameplayAbilities[i];
			if (!klass)
				continue;

			auto* def = static_cast<UGameplayAbility*>(klass->CreateDefaultObject());
			FGameplayAbilitySpecHandle handle{};
			handle.GenerateNewHandle();

			FGameplayAbilitySpec spec{};
			spec.Handle = handle;
			spec.Ability = def;
			spec.Level = 0;
			spec.InputID = -1;
			Give(asc, &handle, spec);
		}
	}

	inline void FireHandle(UAbilitySystemComponent* asc, FGameplayAbilitySpecHandle handle, FPredictionKey key, FGameplayEventData* event = nullptr)
	{
		if (!asc)
			return;

		FGameplayAbilitySpec* spec = nullptr;
		for (int i = 0; i < asc->ActivatableAbilities.Items.Num(); ++i)
		{
			if (asc->ActivatableAbilities.Items[i].Handle.Handle == handle.Handle)
			{
				spec = &asc->ActivatableAbilities.Items[i];
				break;
			}
		}

		if (!spec || !spec->Ability)
		{
			asc->ClientActivateAbilityFailed(handle, key.Current);
			return;
		}

		spec->InputPressed = true;
		UGameplayAbility* instanced = nullptr;
		if (!Fire(asc, handle, key, &instanced, nullptr, event))
		{
			asc->ClientActivateAbilityFailed(handle, key.Current);
			spec->InputPressed = false;
			asc->ActivatableAbilities.MarkItemDirty(*spec);
		}
	}

	inline bool OnTry(UAbilitySystemComponent* asc, FGameplayAbilitySpecHandle handle, bool, FPredictionKey key)
	{
		FireHandle(asc, handle, key);
		return true;
	}

	inline bool OnTryEvent(UAbilitySystemComponent* asc, FGameplayAbilitySpecHandle handle, bool, FPredictionKey key, FGameplayEventData event)
	{
		FireHandle(asc, handle, key, &event);
		return true;
	}

	inline void SeedPlayer(AFortPlayerState* state)
	{
		static auto* gas = Find<UFortAbilitySet>("/Game/Abilities/Player/Generic/Traits/DefaultPlayer/GAS_AthenaPlayer.GAS_AthenaPlayer");
		if (state && state->AbilitySystemComponent)
			AttachSet(state->AbilitySystemComponent, gas);
	}
}
