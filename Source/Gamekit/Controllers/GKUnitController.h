// BSD 3-Clause License
//
// Copyright (c) 2019, Pierre Delaunay
// All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"

#include "GKUnitController.generated.h"

/**
 * Simple AI controller for unit that are not controlled by a the player
 * but could be
 */
UCLASS(Blueprintable)
class GAMEKIT_API AGKUnitAIController : public AAIController
{
	GENERATED_BODY()

public:
	AGKUnitAIController();

};
