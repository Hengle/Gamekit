// BSD 3-Clause License Copyright (c) 2021, Pierre Delaunay All rights reserved.

#pragma once

#include "Items/GKItem.h"
#include "GKSkillItem.generated.h"

/** Native base class for skills, should be blueprinted */
UCLASS(Blueprintable)
class GAMEKIT_API UGKSkillItem : public UGKItem
{
	GENERATED_BODY()

public:
	/** Constructor */
	UGKSkillItem()
	{
		
	}
};