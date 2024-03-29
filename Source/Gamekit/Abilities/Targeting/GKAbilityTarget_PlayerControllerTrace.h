// BSD 3-Clause License Copyright (c) 2021, Pierre Delaunay All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Abilities/Targeting/GKAbilityTarget_Actor.h"
#include "GKAbilityTarget_PlayerControllerTrace.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTargetValidityChanged, const FHitResult&, TraceHit, bool, IsValid);

/**
* Basic Trace for a top down game, we do not really need more than that
/rst
.. note:: 

   ``UAbilitySystemComponent::LocalInputConfirm`` clear all delegate binding when the input is called
   This means if the user pressed the confirm button with an invalid target, the delegate binding
   needs to be set again inside ``AGameplayAbilityTargetActor::ConfirmTargeting``

/endrst
*/
UCLASS(Blueprintable, notplaceable)
class GAMEKIT_API AGKAbilityTarget_PlayerControllerTrace : public AGKAbilityTarget_Actor
{
	GENERATED_UCLASS_BODY()

public:
	virtual void StartTargeting(UGKGameplayAbility* Ability) override;

	virtual void InitializeFromAbilityData(FGKAbilityStatic const& AbilityData);

	virtual void StopTargeting() override;

	virtual void Tick(float DeltaSeconds) override;
	 
	virtual bool IsTargetValid() const;

	//! Is the current target valid ?
	virtual bool IsConfirmTargetingAllowed() override;

	//! Stop targeting
	virtual void CancelTargeting() override;

	virtual void ConfirmTargetingAndContinue() override;

	//! Select the current target as our final target
	virtual void ConfirmTargeting() override;

	//! Listen to user inputs
	virtual void BindToConfirmCancelInputs();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Trace)
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Trace)
	float MaxRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Trace)
	float MinRange;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ExposeOnSpawn = true), Category = Trace)
	float AreaOfEffect;

	UPROPERTY(BlueprintReadOnly)
	FHitResult LatestHitResult;

	UPROPERTY(BlueprintReadOnly)
	FHitResult LatestValidHitResult;

	UPROPERTY(BlueprintReadOnly)
	FVector TraceEndPoint;

	UPROPERTY(BlueprintReadOnly)
	bool bIsTargetValid;

	UPROPERTY(BlueprintAssignable)
	FTargetValidityChanged TargetValidityChanged;

	bool IsInputBound;
};
