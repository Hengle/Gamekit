// BSD 3-Clause License Copyright (c) 2021, Pierre Delaunay All rights reserved.

#include "Abilities/GKGameplayAbility.h"

#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "Engine/NetSerialization.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameplayCue_Types.h"
#include "GameFramework/ProjectileMovementComponent.h"

#include "Gamekit.h"
#include "Abilities/GKAbilitySystemComponent.h"
#include "Abilities/AbilityTasks/GKAbilityTask_PlayMontageAndWaitForEvent.h"
#include "Abilities/Targeting/GKAbilityTarget_PlayerControllerTrace.h"
#include "Abilities/Targeting/GKAbilityTask_WaitTargetData.h"
#include "Abilities/GKTargetType.h"
#include "Abilities/GKAbilitySystemGlobals.h"
#include "Abilities/GKCastPointAnimNotify.h"
#include "Projectiles/GKProjectile.h"
#include "Characters/GKCharacter.h"
#include "Controllers/GKPlayerController.h"


UGKGameplayEffectDyn::UGKGameplayEffectDyn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UGKGameplayAbility::UGKGameplayAbility() {
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
	ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Debuff.Stun")));

	Immediate = false;
	AbilityStatic = nullptr;

	CooldownEffectInstance = nullptr;
	CostEffectInstance = nullptr;
} 

void UGKGameplayAbility::PostInitProperties() {
	Super::PostInitProperties();

	if (AbilityDataTable && AbilityRowName.IsValid()) {
		UE_LOG(LogGamekit, Warning, TEXT("Loading From DataTable"));
		OnDataTableChanged_Native();
	}
}

void UGKGameplayAbility::OnDataTableChanged_Native() {
	// Reset Cache
	AbilityStatic = nullptr;
	bool Valid = false;

	FGKAbilityStatic* AbilityStaticOut = GetAbilityStatic();

	if (!AbilityStaticOut) {
		AbilityStatic = nullptr;
	}
}

void UGKGameplayAbility::LoadFromDataTable(FGKAbilityStatic& AbilityDef) {
	UE_LOG(LogGamekit, Warning, TEXT("Init Ability from table %s"), *AbilityDef.Name.ToString());

	CooldownEffectInstance = NewCooldownEffectFromConfig(AbilityDef.Cooldown);
	CostEffectInstance = NewCostEffectFromConfig(AbilityDef.Cost);
}

FGKAbilityStatic* UGKGameplayAbility::GetAbilityStatic() {
	if (AbilityStatic) {
		return AbilityStatic;
	}

	if (!AbilityDataTable || !AbilityRowName.IsValid()) {
		return nullptr;
	}

	// we do not have a cached value
	if (!AbilityStatic) {
		AbilityStatic = AbilityDataTable->FindRow<FGKAbilityStatic>(AbilityRowName, TEXT("Ability"), true);

		if (AbilityStatic != nullptr) {
			LoadFromDataTable(*AbilityStatic);
		}

		// Listen to data table change
		if (!AbilityDataTable->OnDataTableChanged().IsBoundToObject(this)) {
			AbilityDataTable->OnDataTableChanged().AddUObject(this, &UGKGameplayAbility::OnDataTableChanged_Native);
		}
	}

	return AbilityStatic;
}

void UGKGameplayAbility::K2_GetAbilityStatic(FGKAbilityStatic& AbilityStaticOut, bool& Valid) {
	Valid = false;

	auto Result = GetAbilityStatic();
	if (Result != nullptr) {
		AbilityStaticOut = *Result;
		Valid = true;
	}
}

FGKGameplayEffectContainerSpec UGKGameplayAbility::MakeEffectContainerSpecFromContainer(const FGKGameplayEffectContainer& Container, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	// First figure out our actor info
	FGKGameplayEffectContainerSpec ReturnSpec;
	AActor* OwningActor = GetOwningActorFromActorInfo();
	AGKCharacterBase* OwningCharacter = Cast<AGKCharacterBase>(OwningActor);
	UGKAbilitySystemComponent* OwningASC = UGKAbilitySystemComponent::GetAbilitySystemComponentFromActor(OwningActor);

	if (OwningASC)
	{
		// If we have a target type, run the targeting logic. This is optional, targets can be added later
		if (Container.TargetType.Get())
		{
			TArray<FHitResult> HitResults;
			TArray<AActor*> TargetActors;
			const UGKTargetType* TargetTypeCDO = Container.TargetType.GetDefaultObject();
			AActor* AvatarActor = GetAvatarActorFromActorInfo();
			TargetTypeCDO->GetTargets(OwningCharacter, AvatarActor, EventData, HitResults, TargetActors);
			ReturnSpec.AddTargets(HitResults, TargetActors);
		}

		// If we don't have an override level, use the default on the ability itself
		if (OverrideGameplayLevel == INDEX_NONE)
		{
			OverrideGameplayLevel = OverrideGameplayLevel = this->GetAbilityLevel(); //OwningASC->GetDefaultAbilityLevel();
		}

		// Build GameplayEffectSpecs for each applied effect
		for (const TSubclassOf<UGameplayEffect>& EffectClass : Container.TargetGameplayEffectClasses)
		{
			ReturnSpec.TargetGameplayEffectSpecs.Add(MakeOutgoingGameplayEffectSpec(EffectClass, OverrideGameplayLevel));
		}
	}
	return ReturnSpec;
}

FGKGameplayEffectContainerSpec UGKGameplayAbility::MakeEffectContainerSpec(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	FGKGameplayEffectContainer* FoundContainer = EffectContainerMap.Find(ContainerTag);

	if (FoundContainer)
	{
		return MakeEffectContainerSpecFromContainer(*FoundContainer, EventData, OverrideGameplayLevel);
	}
	return FGKGameplayEffectContainerSpec();
}

TArray<FActiveGameplayEffectHandle> UGKGameplayAbility::ApplyEffectContainerSpec(const FGKGameplayEffectContainerSpec& ContainerSpec)
{
	TArray<FActiveGameplayEffectHandle> AllEffects;

	// Iterate list of effect specs and apply them to their target data
	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{
		AllEffects.Append(K2_ApplyGameplayEffectSpecToTarget(SpecHandle, ContainerSpec.TargetData));
	}
	return AllEffects;
}

TArray<FActiveGameplayEffectHandle> UGKGameplayAbility::ApplyEffectContainer(FGameplayTag ContainerTag, const FGameplayEventData& EventData, int32 OverrideGameplayLevel)
{
	FGKGameplayEffectContainerSpec Spec = MakeEffectContainerSpec(ContainerTag, EventData, OverrideGameplayLevel);
	return ApplyEffectContainerSpec(Spec);
}

// This allow animation to be standard, and Gameplay designed can tweak the CastPoint independently
// from the actual animation
float ResolveAnimationPlayRate(FGKAbilityStatic const& Data, class UAnimMontage* Montage) {
    if (Montage == nullptr) {
        return 1.f;
	}
	float Rate = 1.f;

	TArray<FAnimNotifyEventReference> OutActiveNotifies;
	Montage->GetAnimNotifies(0, Montage->GetPlayLength(), false, OutActiveNotifies);
	for (auto& NotifyRef : OutActiveNotifies) {
		auto NotifyEvent = NotifyRef.GetNotify();
		auto Notify = Cast<UGKCastPointAnimNotify>(NotifyEvent->Notify);

		// Find the ability Cast point on this animation
		// Create a CastPointAnimNotify class and check here
		if (Notify) {
			// if the Notify event is at 2 sec but the cast point is 1 sec
			// play the animation 2x so the Notify is indeed sent after 1 sec
			Rate = NotifyEvent->GetTriggerTime() / Data.CastTime;

			// Realisticly though we can only cast one ability with animation
			// So maybe we can reuse the same tag for everything
			// TODO: Here we could set the Event Tag to match the ability
			// Notify->CastPointEventTag = ;

			// We should probably only have one cast point 
			break;
		}
	}

	return Rate;
}

void UGKGameplayAbility::OnAbilityTargetingCancelled(const FGameplayAbilityTargetDataHandle& Data) {
	TargetingResultDelegate.Broadcast(true);
	TargetTask->EndTask();
	TargetTask = nullptr;
	// ------------------

	K2_CancelAbility();
}

void UGKGameplayAbility::OnAbilityTargetAcquired(const FGameplayAbilityTargetDataHandle& Data) {
	TargetingResultDelegate.Broadcast(false);
	TargetTask->EndTask();
	TargetTask = nullptr;
	// ------------------

	if (Immediate) {
		UGKGameplayAbility::OnAbilityAnimationEvent(FGameplayTag(), FGameplayEventData());
		return;
	}

	// Warning Animation is already playing
	// user need to cancel backswing to be able to cast before
	// animation ends
	// TODO: when implementing Action Queue handle that
	if (IsValid(AnimTask)) {
		AnimTask->EndTask();
		// return;
	}

	auto Montage = GetAnimation();

	// Compute the Animation rate
	// so the CastPoint is triggered at the right time
	if (DyanmicCastPoint) {
		FGKAbilityStatic* AbilityData = GetAbilityStatic();

		if (AbilityData) {
			Rate = ResolveAnimationPlayRate(*AbilityData, Montage);
		}
	}
	
	// ## We are ready to play the animation
	AnimTask = UGKAbilityTask_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(
		this,
		NAME_None,
		Montage,
		// Listen to our ability event, empty matches everything
		FGameplayTagContainer(),
		// Attach target data to the Animation task so it can be forwarded
		// to the activation
		Data,
		Rate,
		StartSection,
		false,
		1.f
	);

	// Channel: Spell is cast after the animation ends BUT
	// resources are committed when the animation starts
	// During the channel the player cannot move.
	// 
	// Normal: Spell is cast after animation reach the cast point
	// and the resource are committed then

	AnimTask->OnBlendOut.AddDynamic(this, &UGKGameplayAbility::OnAbilityAnimationBlendOut);
	AnimTask->OnInterrupted.AddDynamic(this, &UGKGameplayAbility::OnAbilityAnimationAbort);
	AnimTask->OnCancelled.AddDynamic(this, &UGKGameplayAbility::OnAbilityAnimationAbort);
	AnimTask->EventReceived.AddDynamic(this, &UGKGameplayAbility::OnAbilityAnimationEvent);

	// Run
	AnimTask->Activate();
}

void UGKGameplayAbility::OnAbilityAnimationBlendOut(FGameplayTag EventTag, FGameplayEventData EventData) {
	K2_EndAbility();
	AnimTask = nullptr;
}

void UGKGameplayAbility::OnAbilityAnimationAbort(FGameplayTag EventTag, FGameplayEventData EventData) {
	K2_CancelAbility();
	AnimTask = nullptr;
}

bool UGKGameplayAbility::K2_CheckTagRequirements(FGameplayTagContainer& RelevantTags) {
	UAbilitySystemComponent* const AbilitySystemComponent = CurrentActorInfo->AbilitySystemComponent.Get();
	return CheckTagRequirements(*AbilitySystemComponent, &RelevantTags);
}

bool UGKGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const {
	auto ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) {
		return false;
	}

	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);

	if (Spec == nullptr || Spec->Level <= 0) {
		const FGameplayTag& NotYetLearnedTag = ((UGKAbilitySystemGlobals&)UGKAbilitySystemGlobals::Get()).ActivateFailNotYetLearnedTag;
		if (OptionalRelevantTags && NotYetLearnedTag.IsValid())
		{
			OptionalRelevantTags->AddTag(NotYetLearnedTag);
		}

		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UGKGameplayAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) {
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);

	if (AnimTask) {
		AnimTask->ExternalCancel();
	}

	AnimTask = nullptr;
}

void UGKGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const {
	// No generated Effect
	if (!CooldownEffectInstance) {
		// This uses the GameEffect CDO, when the Ability is not instantiated
		return Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
	}

	return ApplyGameplayEffectToOwnerDynamic(
		Handle,
		ActorInfo,
		ActivationInfo,
		CooldownEffectInstance,
		GetAbilityLevel(Handle, ActorInfo),
		1
	);
}

void UGKGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const {
	// No generated Effect
	if (!CostEffectInstance) {
		// This uses the GameEffect CDO, when the Ability is not instantiated
		return Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	}

	return ApplyGameplayEffectToOwnerDynamic(
		Handle,
		ActorInfo,
		ActivationInfo,
		CostEffectInstance,
		GetAbilityLevel(Handle, ActorInfo),
		1
	);
}

void UGKGameplayAbility::ApplyGameplayEffectToOwnerDynamic(const FGameplayAbilitySpecHandle Handle, 
													       const FGameplayAbilityActorInfo* ActorInfo, 
														   const FGameplayAbilityActivationInfo ActivationInfo,  
														   const UGameplayEffect* GameplayEffect, 
														   float GameplayEffectLevel, 
														   int32 Stacks) const 
{
	if (!GameplayEffect || !(HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))) {
		return;
	}

	FScopedGameplayCueSendContext GameplayCueSendContext;

	UAbilitySystemComponent* AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();

	// NOTE: UE is doing this on their side, FGameplayEffectSpecHandle is a shared pointer
	// Let just use the struct as is and see
	// 
	//		FGameplayEffectSpecHandle SpecHandle = new FGameplayEffectSpec
	// 
	auto Spec = FGameplayEffectSpec(
		GameplayEffect,
		MakeEffectContext(Handle, ActorInfo),
		GameplayEffectLevel
	);

	FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	if (AbilitySpec) {
		Spec.SetByCallerTagMagnitudes = AbilitySpec->SetByCallerTagMagnitudes;
	}

	Spec.StackCount = Stacks;

	ApplyAbilityTagsToGameplayEffectSpec(Spec, AbilitySpec);
	
	// ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(Spec, AbilitySystemComponent->GetPredictionKeyForNewAction());
}

void UGKGameplayAbility::OnAbilityAnimationEvent(FGameplayTag EventTag, FGameplayEventData EventData) {
	// Only the authority commits the ability
	// in the BP side I had a double commit issue
	// but it did not happen in C++ :thinking:
	// might be that 2nd replicated CommitAbility was received by the server after the 1st EndAbility was called on the server
	// making the 2nd Commit valid while in C++ the time window for this to happen is much shorter
	// making the bug less likely to happen
	// 
	// Note: cooldowns will prevent the bug from happening
	if (!K2_HasAuthority()) {
		// needs to wait for CommitAbility being replicated on client and then spawn the projectile everywhere
		return;
	}

	/*
	if (EventData.TargetData) {
		
	}
	//*/

	if (K2_CommitAbility()) {
		SpawnProjectile(EventTag, EventData);
		K2_EndAbility();
		return;
	}

	K2_CancelAbility();
}

void UGKGameplayAbility::ActivateAbility_Hidden() {}

void UGKGameplayAbility::ActivateAbility_Passive() {}

void UGKGameplayAbility::ActivateAbility_NoTarget() {
	// Cancel previous actions
	UGKGameplayAbility::OnAbilityTargetAcquired(FGameplayAbilityTargetDataHandle());
}

// The problem here is that we have limited control over the Target Actor
// everything is stashed inside the task
void UGKGameplayAbility::ActivateAbility_ActorTarget() {
	auto TargetTask2 = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		NAME_None,
		EGameplayTargetingConfirmation::UserConfirmed,
		AGKAbilityTarget_PlayerControllerTrace::StaticClass()
	);

	TargetTask2->Cancelled.AddDynamic(this, &UGKGameplayAbility::OnAbilityTargetingCancelled);
	TargetTask2->ValidData.AddDynamic(this, &UGKGameplayAbility::OnAbilityTargetAcquired);
	TargetTask2->Activate();
}

void UGKGameplayAbility::ActivateAbility_PointTarget() {
	AGKPlayerController* PC = Cast<AGKPlayerController>(GetCurrentActorInfo()->PlayerController.Get());

	if (!PC) {
		return;
	}

	auto ASC = Cast<UGKAbilitySystemComponent>(GetCurrentActorInfo()->AbilitySystemComponent.Get());
	if (!ASC) {
		return;
	}

	FGKAbilityStatic* Data = GetAbilityStatic();

	if (!Data || !Data->AbilityTargetActorClass) {
		return;
	}

	auto Target = ASC->GetAbilityTarget_Actor(TSubclassOf<AGKAbilityTarget_Actor>(Data->AbilityTargetActorClass));

	if (!IsValid(Target)) {
		return;
	}

	Target->InitializeFromAbilityData(*Data);

	TargetTask = UGKAbilityTask_WaitForTargetData::WaitForTargetDataUsingActor(
		this,												// UGameplayAbility * OwningAbility, 
		NAME_None,											// FName TaskInstanceName
		EGameplayTargetingConfirmation::UserConfirmed,		// ConfirmationType
		Target											 	// TargetActor
	);

	TargetingStartDelegate.Broadcast();
	TargetTask->Cancelled.AddDynamic(this, &UGKGameplayAbility::OnAbilityTargetingCancelled);
	TargetTask->ValidData.AddDynamic(this, &UGKGameplayAbility::OnAbilityTargetAcquired);
	TargetTask->Activate();
}

void UGKGameplayAbility::ActivateAbility_Toggle() {
	if (IsActive()) {
		// Does not play animation on toggle off
		K2_EndAbility();
	}
	else {
		// Plays animation on toggle on
		UGKGameplayAbility::OnAbilityTargetAcquired(FGameplayAbilityTargetDataHandle());
	}
};

void UGKGameplayAbility::ActivateAbility_Native() {
	auto AbilityData = GetAbilityStatic();

	if (!AbilityData) {
		return;
	}

	switch (AbilityData->AbilityBehavior) {
	case EGK_AbilityBehavior::ActorTarget:	return ActivateAbility_ActorTarget();
	case EGK_AbilityBehavior::Hidden:		return ActivateAbility_Hidden();
	case EGK_AbilityBehavior::NoTarget:		return ActivateAbility_NoTarget();
	case EGK_AbilityBehavior::Passive:		return ActivateAbility_Passive();
	case EGK_AbilityBehavior::PointTarget:	return ActivateAbility_PointTarget();
	case EGK_AbilityBehavior::Toggle:		return ActivateAbility_Toggle();
	}
}

const FGameplayTagContainer& UGKGameplayAbility::GetAbilityCooldownTags() const {
	return *Super::GetCooldownTags();
}

TArray<FGameplayAttribute> UGKGameplayAbility::GetAbilityCostAttribute() const {
	TArray<FGameplayAttribute> Out;
	auto CostEffect = GetCostGameplayEffect();
	for (auto& Modifiers : CostEffect->Modifiers)
	{
		Out.Add(Modifiers.Attribute);
	}
	return Out;
}

void UGKGameplayAbility::LevelUpAbility() {
	if (IsInstantiated() == false || CurrentActorInfo == nullptr){
		return;
	}

	auto ASC = Cast<UGKAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
	
	if (ASC) {
		ASC->LevelUpAbility(CurrentSpecHandle);
	}
}

UGameplayEffect* UGKGameplayAbility::GetCooldownGameplayEffect() const {
	if (CooldownEffectInstance) {
		UE_LOG(LogGamekit, Verbose, TEXT("Returns generated cooldown effect"));
		return CooldownEffectInstance;
	}

	return Super::GetCooldownGameplayEffect();
}

UGameplayEffect* UGKGameplayAbility::GetCostGameplayEffect() const {
	if (CostEffectInstance) {
		UE_LOG(LogGamekit, Verbose, TEXT("Returns generated cost effect"));
		return CostEffectInstance;
	}

	return Super::GetCostGameplayEffect();
}

UGameplayEffect* UGKGameplayAbility::NewCooldownEffectFromConfig(TArray<float>& Durations) {
	if (Durations.Num() <= 0) {
		UE_LOG(LogGamekit, Warning, TEXT("Cooldown value is not set!"));
		return nullptr;
	}

	auto RowName = FName(FString("Cooldown.") + AbilityStatic->Name.ToString());

	UGameplayEffect* CooldownEffect = NewObject<UGKGameplayEffectDyn>(this /*, TEXT("GameplayEffect.Cooldown") */);
	CooldownEffect->DurationPolicy = EGameplayEffectDurationType::HasDuration;

	auto ScalableDuration = GenerateCurveDataFromArray(RowName, Durations, true, false);
	CooldownEffect->DurationMagnitude = ScalableDuration;

	CooldownEffect->InheritableOwnedTagsContainer.AddTag(
		FGameplayTag::RequestGameplayTag(RowName)
	);

	return CooldownEffect;
};

UGameplayEffect* UGKGameplayAbility::NewCostEffectFromConfig(FGKAbilityCost& Conf) {
	if (Conf.Value.Num() <= 0) {
		UE_LOG(LogGamekit, Warning, TEXT("Cost value is not set!"));
		return nullptr;
	}

	auto RowName = FName(FString("Cost.") + AbilityStatic->Name.ToString());

	UGameplayEffect* CostEffect = NewObject<UGKGameplayEffectDyn>(this /*, TEXT("GameplayEffect.Cost")*/);
	CostEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
	CostEffect->Modifiers.SetNum(1);

	FGameplayModifierInfo& AttributeCost = CostEffect->Modifiers[0];

	auto ScalableCost = GenerateCurveDataFromArray(RowName, Conf.Value, true, true);
	AttributeCost.ModifierMagnitude = ScalableCost;
	AttributeCost.ModifierOp = EGameplayModOp::Additive;
	AttributeCost.Attribute = Conf.Attribute;

	return CostEffect;
};

UGameplayEffect* NewPassiveRegenEffect(UObject* Parent, FGameplayAttribute Attribute, float Value, float Period, FName name) {
	if (Value <= 0) {
		return nullptr;
	}

	// Note that Giving it a name HERE makes the ability not work
	// Health Regen woudl work but mana would not
	UGameplayEffect* Regen = NewObject<UGKGameplayEffectDyn>(Parent, name /*, TEXT("GameplayEffect.Regen")*/);
	Regen->DurationPolicy = EGameplayEffectDurationType::Infinite;
	Regen->Period = Period;
	Regen->Modifiers.SetNum(1);

	FGameplayModifierInfo& RegenModifier = Regen->Modifiers[0];

	RegenModifier.ModifierMagnitude = FScalableFloat(Value * Period);
	RegenModifier.ModifierOp = EGameplayModOp::Additive;
	RegenModifier.Attribute = Attribute;

	auto TagName = FName(FString("Regen.") + Attribute.GetName());
	Regen->InheritableOwnedTagsContainer.AddTag(
		FGameplayTag::RequestGameplayTag(TagName)
	);

	return Regen;
};

FScalableFloat UGKGameplayAbility::GenerateCurveDataFromArray(FName RowName, TArray<float>& Values, bool ValuesAreFinal, bool Cost) {
	UCurveTable* Table = UAbilitySystemGlobals::Get().GetGlobalCurveTable();
	if (Table == nullptr) {
		UE_LOG(LogGamekit, Warning, TEXT("UAbilitySystemGlobals::GlobalCurveTableName is not set cannot generate curve data %s"), *RowName.ToString());
		return FScalableFloat();
	}

	if (Values.Num() <= 0) {
		UE_LOG(LogGamekit, Warning, TEXT("GenerateCurveDataFromDataTable cannot generate a curve with no values %s"), *RowName.ToString());
		return FScalableFloat();
	}

	if (Values.Num() == 1) {
		float BaseValue = Values[0];
		if (Cost) {
			BaseValue = -abs(BaseValue);
		}
		return FScalableFloat(BaseValue);
	}

	FSimpleCurve& Curve = Table->AddSimpleCurve(RowName);

	for(int i = 1 - int(ValuesAreFinal); i < Values.Num(); i++){
		float v;

		if (ValuesAreFinal) {
			v = Values[i] / Values[0];
		}
		else {
			v = Values[i];
		}

		Curve.AddKey(i + 1, v);
	}

	float BaseValue = Values[0];
	if (Cost) {
		BaseValue = -abs(BaseValue);
	}

	FScalableFloat ScalableFloat(BaseValue);
	ScalableFloat.Curve.RowName = RowName;
	ScalableFloat.Curve.CurveTable = Table;
	return ScalableFloat;
}

FGameplayTagContainer UGKGameplayAbility::GetActivationBlockedTag() {
	return ActivationBlockedTags;
}

FGameplayTagContainer UGKGameplayAbility::GetActivationRequiredTag() {
	return ActivationRequiredTags;
}

void UGKGameplayAbility::SpawnProjectile(FGameplayTag EventTag, FGameplayEventData EventData) {
	FGKAbilityStatic* Data = GetAbilityStatic();

	if (!Data || !Data->ProjectileActorClass) {
		return;
	}

	auto& TargetData = EventData.TargetData;
	if (TargetData.Num() <= 0) {
		UE_LOG(LogGamekit, Warning, TEXT("UGKGameplayAbility::SpawnProjectile: No TargetData"));
		return;
	}

	auto ActorInfo = GetCurrentActorInfo();
	auto Actor = ActorInfo->AvatarActor;
	auto Pawn = Cast<APawn>(Actor.Get());

	// From: GameplayAbilities\Public\Abilities\Tasks\AbilityTask_SpawnActor.h
	// > Long term we can also use this task as a sync point.
	// > If the executing client could wait execution until the server createsand replicate sthe
	// > actor down to him.We could potentially also use this to do predictive actor spawning / reconciliation.
	//
	// Pending UE4 does it we might have to do it here
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.Owner = Pawn;
	SpawnInfo.Instigator = Pawn;
	SpawnInfo.bDeferConstruction = true;

	auto ActorRot = Actor->GetActorRotation();
	FTransform Transform;
	Transform.SetRotation(FQuat(ActorRot));
	Transform.SetLocation(Actor->GetActorLocation() + Actor->GetActorForwardVector() * 64.0f);

	auto ProjectileInstance = Actor->GetWorld()->SpawnActor<AGKProjectile>(
		Data->ProjectileActorClass,
		Transform,
		SpawnInfo
	);

	ProjectileInstance->Direction = ActorRot.Vector();
	ProjectileInstance->Speed = Data->ProjectileSpeed;
	ProjectileInstance->Target = TargetData.Get(0)->GetHitResult()->Actor.Get();
	ProjectileInstance->Behavior = Data->ProjectileBehavior;
	ProjectileInstance->Range = Data->ProjectileRange;
	ProjectileInstance->GameplayEffects = MakeEffectContainerSpec(EventTag, EventData);
	// The attachment is probably Character/class/Skeleton defined
	// ...

	UGameplayStatics::FinishSpawningActor(ProjectileInstance, Transform);

	// we need a multi-cast here to tell people we are ready to make the Projectile move
	// this would assume that the network works with a queue so given we modified the properties
	// earlier they will be updated first
	// 
	// ProjectileInstance->Ready()
}

UAnimMontage* UGKGameplayAbility::GetAnimation() {
	if (AnimMontages.Animations.Num() > 0) {
		return AnimMontages.Sample();
	}

	auto ActorInfo = GetCurrentActorInfo();
	auto Actor = ActorInfo->AvatarActor;
	auto Pawn = Cast<AGKCharacterBase>(Actor.Get());

	if (!Pawn) {
		UE_LOG(LogGamekit, Warning, TEXT("UGKGameplayAbility::GetAnimation could not fetch pawn animation for cast ability"));
		return nullptr;
	}

	auto UnitConf = Pawn->GetUnitStatic();
	auto AbilityConf = GetAbilityStatic();

	if (UnitConf && AbilityConf) {
		AnimMontages = UnitConf->AnimationSet.GetAnimations(AbilityConf->AbilityAnimation);
	}

	return AnimMontages.Sample();
}