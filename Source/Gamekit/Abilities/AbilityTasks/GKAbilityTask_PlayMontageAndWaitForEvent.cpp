// BSD 3-Clause License Copyright (c) 2021, Pierre Delaunay All rights reserved.

#include "Abilities/AbilityTasks/GKAbilityTask_PlayMontageAndWaitForEvent.h"

#include "Abilities/GKAbilitySystemComponent.h"

#include "GameFramework/Character.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"

UGKAbilityTask_PlayMontageAndWaitForEvent::UGKAbilityTask_PlayMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Rate = 1.f;
	bStopWhenAbilityEnds = true;
}

UGKAbilitySystemComponent* UGKAbilityTask_PlayMontageAndWaitForEvent::GetTargetASC()
{
	return Cast<UGKAbilitySystemComponent>(AbilitySystemComponent);
}

void UGKAbilityTask_PlayMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay)
	{
		if (Montage == MontageToPlay)
		{
			AbilitySystemComponent->ClearAnimatingAbility(Ability);

			// Reset AnimRootMotionTranslationScale
			ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
			if (Character && (Character->GetLocalRole() == ROLE_Authority ||
							  (Character->GetLocalRole() == ROLE_AutonomousProxy && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
			{
				Character->SetAnimRootMotionTranslationScale(1.f);
			}

		}
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
	else
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}

void UGKAbilityTask_PlayMontageAndWaitForEvent::OnAbilityCancelled()
{
	// TODO: Merge this fix back to engine, it was calling the wrong callback

	if (StopPlayingMontage())
	{
		// Let the BP handle the interrupt as well
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
}

void UGKAbilityTask_PlayMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	EndTask();
}

void UGKAbilityTask_PlayMontageAndWaitForEvent::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;
		TempData.TargetData = TargetData;
		EventReceived.Broadcast(EventTag, TempData);
	}
}

UGKAbilityTask_PlayMontageAndWaitForEvent* UGKAbilityTask_PlayMontageAndWaitForEvent::PlayMontageAndWaitForEvent(UGameplayAbility* OwningAbility,
																												 FName TaskInstanceName, 
																												 UAnimMontage* MontageToPlay, 
																												 FGameplayTagContainer EventTags, 
																												 const FGameplayAbilityTargetDataHandle& TargetData,
																												 float Rate, 
																												 FName StartSection, 
																												 bool bStopWhenAbilityEnds, 
																												 float AnimRootMotionTranslationScale)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(Rate);

	UGKAbilityTask_PlayMontageAndWaitForEvent* MyObj = NewAbilityTask<UGKAbilityTask_PlayMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);
	MyObj->MontageToPlay = MontageToPlay;
	MyObj->EventTags = EventTags;
	MyObj->Rate = Rate;
	MyObj->TargetData = TargetData;
	MyObj->StartSection = StartSection;
	MyObj->AnimRootMotionTranslationScale = AnimRootMotionTranslationScale;
	MyObj->bStopWhenAbilityEnds = bStopWhenAbilityEnds;

	return MyObj;
}

void UGKAbilityTask_PlayMontageAndWaitForEvent::Activate()
{
	if (Ability == nullptr)
	{
		return;
	}

	bool bPlayedMontage = false;
	UGKAbilitySystemComponent* GKAbilitySystemComponent = GetTargetASC();

	if (!GKAbilitySystemComponent){
		ABILITY_LOG(Warning, TEXT("UGKAbilityTask_PlayMontageAndWaitForEvent called on invalid AbilitySystemComponent"));
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

	if (AnimInstance == nullptr) {
		ABILITY_LOG(Warning, TEXT("UGKAbilityTask_PlayMontageAndWaitForEvent call to AnimInstance is null!"));
	}

	// Catch our AnimNotify here to start activating the ability
	EventHandle = GKAbilitySystemComponent->AddGameplayEventTagContainerDelegate(
		EventTags, 
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UGKAbilityTask_PlayMontageAndWaitForEvent::OnGameplayEvent)
	);

	// This will not trigger callbacks 
	GKAbilitySystemComponent->CurrentMontageStop();
	
	// Play the animation
	bPlayedMontage = GKAbilitySystemComponent->PlayMontage(
		Ability, 
		Ability->GetCurrentActivationInfo(), 
		MontageToPlay, 
		Rate, 
		StartSection) > 0.f;

	if (!bPlayedMontage) {
		ABILITY_LOG(Warning, TEXT("UGKAbilityTask_PlayMontageAndWaitForEvent called in Ability %s failed to play montage %s; Task Instance Name %s."), *Ability->GetName(), *GetNameSafe(MontageToPlay), *InstanceName.ToString());
		
		if (ShouldBroadcastAbilityTaskDelegates()){
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	// Playing a montage could potentially fire off a callback into game code which could kill this ability! Early out if we are  pending kill.
	if (ShouldBroadcastAbilityTaskDelegates() == false) {
		return;
	}

	CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &UGKAbilityTask_PlayMontageAndWaitForEvent::OnAbilityCancelled);

	BlendingOutDelegate.BindUObject(this, &UGKAbilityTask_PlayMontageAndWaitForEvent::OnMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

	MontageEndedDelegate.BindUObject(this, &UGKAbilityTask_PlayMontageAndWaitForEvent::OnMontageEnded);
	AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());

	if (Character && (Character->GetLocalRole() == ROLE_Authority ||
				     (Character->GetLocalRole() == ROLE_AutonomousProxy && 
					  Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalPredicted)))
	{
		Character->SetAnimRootMotionTranslationScale(AnimRootMotionTranslationScale);
	}

	SetWaitingOnAvatar();
}

void UGKAbilityTask_PlayMontageAndWaitForEvent::ExternalCancel()
{
	check(AbilitySystemComponent);

	OnAbilityCancelled();

	Super::ExternalCancel();
}

void UGKAbilityTask_PlayMontageAndWaitForEvent::OnDestroy(bool AbilityEnded)
{
	// Note: Clearing montage end delegate isn't necessary since its not a multicast and will be cleared when the next montage plays.
	// (If we are destroyed, it will detect this and not do anything)

	// This delegate, however, should be cleared as it is a multicast
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(CancelledHandle);
		if (AbilityEnded && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	UGKAbilitySystemComponent* GKAbilitySystemComponent = GetTargetASC();
	if (GKAbilitySystemComponent)
	{
		GKAbilitySystemComponent->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

bool UGKAbilityTask_PlayMontageAndWaitForEvent::StopPlayingMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	// Check if the montage is still playing
	// The ability would have been interrupted, in which case we should automatically stop the montage
	if (AbilitySystemComponent && Ability)
	{
		if (AbilitySystemComponent->GetAnimatingAbility() == Ability
			&& AbilitySystemComponent->GetCurrentMontage() == MontageToPlay)
		{
			// Unbind delegates so they don't get called as well
			FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay);
			if (MontageInstance)
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			AbilitySystemComponent->CurrentMontageStop();
			return true;
		}
	}

	return false;
}

FString UGKAbilityTask_PlayMontageAndWaitForEvent::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();

		if (AnimInstance != nullptr)
		{
			PlayingMontage = AnimInstance->Montage_IsActive(MontageToPlay) ? MontageToPlay : AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("PlayMontageAndWaitForEvent. MontageToPlay: %s  (Currently Playing): %s"), *GetNameSafe(MontageToPlay), *GetNameSafe(PlayingMontage));
}
