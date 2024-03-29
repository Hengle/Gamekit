// BSD 3-Clause License Copyright (c) 2019, Pierre Delaunay All rights reserved.

#include "Gamekit/Quest/GKQuestComponent.h"

#include "AbilitySystemComponent.h"

// Sets default values for this component's properties
UGKQuestComponent::UGKQuestComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	// PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UGKQuestComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UGKQuestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}



void UGKQuestComponent::GiveQuest(FName QuestName) {
	FGameplayTagContainer EventTags;

	AbilitySystemComponent->AddGameplayEventTagContainerDelegate(
		EventTags,
		FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UGKQuestComponent::OnGameplayQuestEvent)
	);
}

//*
void UGKQuestComponent::OnGameplayQuestEvent(FGameplayTag Tag, const FGameplayEventData* Payload) {

}
//*/