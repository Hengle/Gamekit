// BSD 3-Clause License
//
// Copyright (c) 2019, Pierre Delaunay
// All rights reserved.

#include "GKMovementUtility.h"

#include "AIController.h"
#include "NavigationPath.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

#define LOCTEXT_NAMESPACE "GKMovementUtility"

UPathFollowingComponent* InitNavigationControl(AController& Controller, float AcceptanceRadius)
{
	AAIController* AsAIController = Cast<AAIController>(&Controller);
	UPathFollowingComponent* PathFollowingComp = nullptr;

	if (AsAIController)
	{
		PathFollowingComp = AsAIController->GetPathFollowingComponent();
	}
	else
	{
		PathFollowingComp = Controller.FindComponentByClass<UPathFollowingComponent>();
		if (PathFollowingComp == nullptr)
		{
			PathFollowingComp = NewObject<UPathFollowingComponent>(&Controller);
			PathFollowingComp->RegisterComponentWithWorld(Controller.GetWorld());
			PathFollowingComp->Initialize();
		}
	}

	PathFollowingComp->SetAcceptanceRadius(AcceptanceRadius);
	return PathFollowingComp;
}

void UGKMovementUtility::SimpleMoveToLocationExact(AController* Controller, const FVector& GoalLocation, float AcceptanceRadius)
{
	UNavigationSystemV1* NavSys = Controller ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(Controller->GetWorld()) : nullptr;
	if (NavSys == nullptr || Controller == nullptr || Controller->GetPawn() == nullptr)
	{
		UE_LOG(LogNavigation, Warning, TEXT("UNavigationSystemV1::SimpleMoveToActor called for NavSys:%s Controller:%s controlling Pawn:%s (if any of these is None then there's your problem"),
					 *GetNameSafe(NavSys), *GetNameSafe(Controller), Controller ? *GetNameSafe(Controller->GetPawn()) : TEXT("NULL"));
		return;
	}

	UPathFollowingComponent* PFollowComp = InitNavigationControl(*Controller, AcceptanceRadius);

	if (PFollowComp == nullptr)
	{
		FMessageLog("PIE").Warning(FText::Format(
				LOCTEXT("SimpleMoveErrorNoComp", "SimpleMove failed for {0}: missing components"),
				FText::FromName(Controller->GetFName())
				));
		return;
	}

	if (!PFollowComp->IsPathFollowingAllowed())
	{
		FMessageLog("PIE").Warning(FText::Format(
				LOCTEXT("SimpleMoveErrorMovement", "SimpleMove failed for {0}: movement not allowed"),
				FText::FromName(Controller->GetFName())
				));
		return;
	}

	const bool bAlreadyAtGoal = PFollowComp->HasReached(GoalLocation, EPathFollowingReachMode::ExactLocation);

	// script source, keep only one move request at time
	if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
	{
		PFollowComp->AbortMove(
				*NavSys,
				FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest,
				FAIRequestID::AnyRequest,
				bAlreadyAtGoal ? EPathFollowingVelocityMode::Reset : EPathFollowingVelocityMode::Keep);
	}

	// script source, keep only one move request at time
	if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
	{
		PFollowComp->AbortMove(
				*NavSys,
				FPathFollowingResultFlags::ForcedScript | FPathFollowingResultFlags::NewRequest);
	}

	if (bAlreadyAtGoal)
	{
		PFollowComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
	}
	else
	{
		const FVector AgentNavLocation = Controller->GetNavAgentLocation();
		const ANavigationData* NavData = NavSys->GetNavDataForProps(Controller->GetNavAgentPropertiesRef(), AgentNavLocation);
		if (NavData)
		{
			FPathFindingQuery Query(Controller, *NavData, AgentNavLocation, GoalLocation);
			FPathFindingResult Result = NavSys->FindPathSync(Query);
			if (Result.IsSuccessful())
			{
				PFollowComp->RequestMove(FAIMoveRequest(GoalLocation), Result.Path);
			}
			else if (PFollowComp->GetStatus() != EPathFollowingStatus::Idle)
			{
				PFollowComp->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
			}
		}
	}
}

bool UGKMovementUtility::SimpleYawTurn(UCharacterMovementComponent* Movement, FRotator DesiredRotation, float DeltaTime, FRotator& FinalRotation) {
	if (!Movement) {
		return false;
	}
	
	FRotator Rot = Movement->GetLastUpdateRotation();

	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(DesiredRotation, Rot);
	DeltaRot.Pitch = 0.f;
	DeltaRot.Roll = 0.f;

	if (DeltaRot.IsNearlyZero()) {
		return false;
	}

	FRotator MaxRotationRate = Movement->RotationRate * DeltaTime;
	float MaxIsGreater = float(MaxRotationRate.Yaw > DeltaRot.Yaw);

	Rot.Yaw += MaxIsGreater * DeltaRot.Yaw + (1 - MaxIsGreater) * MaxRotationRate.Yaw;
	FinalRotation = Rot;

	Movement->MoveUpdatedComponent(FVector::ZeroVector, Rot, false);
	return true;
}
