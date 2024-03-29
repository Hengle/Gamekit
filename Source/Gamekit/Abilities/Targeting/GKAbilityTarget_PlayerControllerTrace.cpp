// BSD 3-Clause License Copyright (c) 2021, Pierre Delaunay All rights reserved.


#include "Abilities/Targeting/GKAbilityTarget_PlayerControllerTrace.h"
#include "Abilities/GKAbilityStatic.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "DrawDebugHelpers.h"
#include "Components/DecalComponent.h"

AGKAbilityTarget_PlayerControllerTrace::AGKAbilityTarget_PlayerControllerTrace(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer) 
{
    // NOTE: Gamekit is deprecating this
    bDestroyOnConfirmation = false;
    // ---------------------------------

    bTickEnabled = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    IsInputBound = false;
}

void AGKAbilityTarget_PlayerControllerTrace::InitializeFromAbilityData(FGKAbilityStatic const& AbilityData) {
    MaxRange = AbilityData.CastMaxRange;
    MinRange = AbilityData.CastMinRange;
    AreaOfEffect = AbilityData.AreaOfEffect;
    ObjectTypes = AbilityData.TargetObjectTypes;

    Super::InitializeFromAbilityData(AbilityData);
}

void AGKAbilityTarget_PlayerControllerTrace::StartTargeting(UGKGameplayAbility* Ability) {
    Super::StartTargeting(Ability);

    SourceActor = OwningAbility->GetActorInfo().AvatarActor.Get();

    // Init the targeting before the tick
    TargetValidityChanged.Broadcast(LatestHitResult, bIsTargetValid);
}

void AGKAbilityTarget_PlayerControllerTrace::Tick(float DeltaSeconds) {
    // Server and launching client only
    if (!OwningAbility) {
        return;
    }

    APlayerController* PC = OwningAbility->GetCurrentActorInfo()->PlayerController.Get();
    check(PC);

    // PC->GetHitResultUnderCursor(ECC_Visibility, false, LatestHitResult);
    // PC->GetHitResultUnderCursorByChannel(ETraceTypeQuery::, false, LatestHitResult);

    PC->GetHitResultUnderCursorForObjects(ObjectTypes, false, LatestHitResult);

    TraceEndPoint = LatestHitResult.Component.IsValid() ? LatestHitResult.ImpactPoint : LatestHitResult.TraceEnd;

    auto valid = IsConfirmTargetingAllowed();
    if (valid) {
        LatestValidHitResult = LatestHitResult;
    }

#if ENABLE_DRAW_DEBUG
    if (bDebug)
    {
        auto Color = valid ? FColor::Green : FColor::Red;
        DrawDebugLine(GetWorld(), SourceActor->GetActorLocation(), TraceEndPoint, Color, false);
        DrawDebugSphere(GetWorld(), TraceEndPoint, 16, 10, Color, false);
        DrawDebugCircle(
            GetWorld(), 
            SourceActor->GetActorLocation() + FVector(0.f, 0.f, 1.f),
            MaxRange,
            32,
            FColor::Green,
            false,
            -1.f,
            0, 
            1,
            FVector(0.f, 1.f, 0.f), 
            FVector(1.f, 0.f, 0.f)
        );
    }
#endif // ENABLE_DRAW_DEBUG

    SetActorLocationAndRotation(TraceEndPoint, SourceActor->GetActorRotation());
}

bool AGKAbilityTarget_PlayerControllerTrace::IsTargetValid() const {
    if (!SourceActor) {
        return false;
    }

    float DistanceSqr = FVector::DistSquared(SourceActor->GetActorLocation(), TraceEndPoint);
    return MaxRange * MaxRange >= DistanceSqr && DistanceSqr >= MinRange * MinRange;
} 

bool AGKAbilityTarget_PlayerControllerTrace::IsConfirmTargetingAllowed() {

    bool newValidity = IsTargetValid();

    if (newValidity != bIsTargetValid) {
        TargetValidityChanged.Broadcast(LatestHitResult, newValidity);
        bIsTargetValid = newValidity;
    }

    return newValidity;
}

void AGKAbilityTarget_PlayerControllerTrace::CancelTargeting() {
    CanceledDelegate.Broadcast(FGameplayAbilityTargetDataHandle());
}

//! Select the current target as our final target
void AGKAbilityTarget_PlayerControllerTrace::ConfirmTargeting() {
    if (!IsConfirmTargetingAllowed())
    {
        // Everytime confirmed input is pressed the callbacks are cleared.
        // if we have not been able to produce a valid target now we need to re-register our inputs
        if (!GenericDelegateBoundASC) {
            return;
        }

        AbilitySystemComponent->GenericLocalConfirmCallbacks.AddDynamic(this, &AGKAbilityTarget_PlayerControllerTrace::ConfirmTargeting);
        return;
    }

    ConfirmTargetingAndContinue();
}

void AGKAbilityTarget_PlayerControllerTrace::ConfirmTargetingAndContinue() {
    check(ShouldProduceTargetData());

    if (!IsConfirmTargetingAllowed()) {
        return;
    }

    // Target is ready, send the data now
    FGameplayAbilityTargetDataHandle Handle = StartLocation.MakeTargetDataHandleFromHitResult(OwningAbility, LatestHitResult);
    TargetDataReadyDelegate.Broadcast(Handle);
}

void AGKAbilityTarget_PlayerControllerTrace::BindToConfirmCancelInputs() {
    check(OwningAbility);

    if (!AbilitySystemComponent){
        ABILITY_LOG(Warning, TEXT("AGameplayAbilityTargetActor::BindToConfirmCancelInputs called with null ASC! Actor %s"), *GetName())
        return;
    }

    const FGameplayAbilityActorInfo* Info = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr);

    // Client
    if (Info && Info->IsLocallyControlled())
    {
        // We have to wait for the callback from the AbilitySystemComponent. Which will always be instigated locally
        AbilitySystemComponent->GenericLocalConfirmCallbacks.AddDynamic(this, &AGameplayAbilityTargetActor::ConfirmTargeting);	// Tell me if the confirm input is pressed
        AbilitySystemComponent->GenericLocalCancelCallbacks.AddDynamic(this, &AGameplayAbilityTargetActor::CancelTargeting);	// Tell me if the cancel input is pressed

        // Save off which ASC we bound so that we can error check that we're removing them later
        GenericDelegateBoundASC = AbilitySystemComponent;
        IsInputBound = true;
        return;
    }

    // Server
    FGameplayAbilitySpecHandle Handle = OwningAbility->GetCurrentAbilitySpecHandle();
    FPredictionKey PredKey = OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey();

    // Setup replication for Confirm & Cancel
    GenericConfirmHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(
        EAbilityGenericReplicatedEvent::GenericConfirm, 
        Handle, 
        PredKey
    ).AddUObject(this, &AGameplayAbilityTargetActor::ConfirmTargeting);

    GenericCancelHandle = AbilitySystemComponent->AbilityReplicatedEventDelegate(
        EAbilityGenericReplicatedEvent::GenericCancel, 
        Handle, 
        PredKey
    ).AddUObject(this, &AGameplayAbilityTargetActor::CancelTargeting);

    IsInputBound = true;

    // if the input was already sent force replication now
    // Calls a given Generic Replicated Event delegate if the event has already been sent 
    if (AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::GenericConfirm, Handle, PredKey)) {
        return;
    }

    if (AbilitySystemComponent->CallReplicatedEventDelegateIfSet(EAbilityGenericReplicatedEvent::GenericCancel, Handle, PredKey)) {
        return;
    }
}

void AGKAbilityTarget_PlayerControllerTrace::StopTargeting() {
    Super::StopTargeting();

    if (!IsInputBound) {
        return;
    }

    if (!AbilitySystemComponent) {
        ABILITY_LOG(Warning, TEXT("AGameplayAbilityTargetActor::CleanupBindings called with null ASC! Actor %s"), *GetName());
        return;
    }

    const FGameplayAbilityActorInfo* Info = (OwningAbility ? OwningAbility->GetCurrentActorInfo() : nullptr);

    // i.e Locally Controlled
    if (GenericDelegateBoundASC && Info && Info->IsLocallyControlled()){
        // We must remove ourselves from GenericLocalConfirmCallbacks/GenericLocalCancelCallbacks, 
        // since while these are bound they will inhibit any *other* abilities
        // that are bound to the same key.
        AbilitySystemComponent->GenericLocalConfirmCallbacks.RemoveDynamic(this, &AGameplayAbilityTargetActor::ConfirmTargeting);
        AbilitySystemComponent->GenericLocalCancelCallbacks.RemoveDynamic(this, &AGameplayAbilityTargetActor::CancelTargeting);

        // Error checking that we have removed delegates from the same ASC we bound them to
        // FIXME: how could this ever be !=
        ensure(GenericDelegateBoundASC == Info->AbilitySystemComponent.Get());
        return;
    }

    // Server
    // Remove Replicated events
    // Remove Confirm event
    AbilitySystemComponent->AbilityReplicatedEventDelegate(
        EAbilityGenericReplicatedEvent::GenericConfirm,
        OwningAbility->GetCurrentAbilitySpecHandle(),
        OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey()
    ).Remove(GenericConfirmHandle);

    // Remove Cancel event
    AbilitySystemComponent->AbilityReplicatedEventDelegate(
        EAbilityGenericReplicatedEvent::GenericCancel,
        OwningAbility->GetCurrentAbilitySpecHandle(),
        OwningAbility->GetCurrentActivationInfo().GetActivationPredictionKey()
    ).Remove(GenericCancelHandle);
}
