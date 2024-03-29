// BSD 3-Clause License Copyright (c) 2021, Pierre Delaunay All rights reserved.

#include "Characters/GKCharacter.h"

#include "Abilities/GKAbilityInputs.h"
#include "Abilities/GKGameplayAbility.h"
#include "Abilities/GKAbilityStatic.h"
#include "Items/GKItem.h"

#include "AbilitySystemGlobals.h"
#include "GameFramework/CharacterMovementComponent.h"


AGKCharacterBase::AGKCharacterBase()
{
	// Create ability system component, and set it to be explicitly replicated
	AbilitySystemComponent = CreateDefaultSubobject<UGKAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Create the attribute set, this replicates by default
	AttributeSet = CreateDefaultSubobject<UGKAttributeSet>(TEXT("AttributeSet"));

	CharacterLevel = 1;
	InputsBound = false;
}

void AGKCharacterBase::OnDataTableChanged_Native() {
	// Reset Cache
	UnitStatic = nullptr;
	GetUnitStatic();
}

void AGKCharacterBase::PostInitProperties() {
	Super::PostInitProperties();

	/*
	if (AttributeSet == nullptr) {
		UE_LOG(LogGamekit, Warning, TEXT("AttributeSet is Invalid"));
		return;
	}

	if (UnitDataTable && UnitRowName.IsValid()) {
		UE_LOG(LogGamekit, Warning, TEXT("Loading Unit Config From DataTable"));
		OnDataTableChanged_Native();
	}*/
}

void AGKCharacterBase::AddPassiveEffect(UGameplayEffect* Effect) {
	if (Effect == nullptr) {
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(this);

	auto ManaSpec = FGameplayEffectSpec(Effect, EffectContext, 1);
	FActiveGameplayEffectHandle ActiveHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(ManaSpec);

	// NOTE: in examples:
	// FGameplayEffectSpecHandle ManaSpecSpecHandle = new FGameplayEffectSpec(Effect, EffectContext, 1);
	
	// ManaSpecSpecHandle is a shared pointer not sure if this is really necessary 
	// could be referenced somewhere at somepoint but until proven necessary do not make a shared pointer
	
	// ApplyGameplayEffectSpecToTarget just calls ApplyGameplayEffectSpecToSelf
	// FActiveGameplayEffectHandle ActiveGEHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToTarget(*ManaSpecSpecHandle.Data.Get(), AbilitySystemComponent);
}

void AGKCharacterBase::LoadFromDataTable(FGKUnitStatic& UnitDef) {
	if (GetLocalRole() != ROLE_Authority || AbilitySystemComponent == nullptr) {
		UE_LOG(LogGamekit, Warning, TEXT("Not Authority"));
		return;
	}

	if (!AttributeSet || AbilitySystemComponent->IsInitialized()) {
		return;
	}

	AttributeSet->SetMaxHealth(UnitDef.Health);
	AttributeSet->SetMaxMana(UnitDef.Mana);

	auto ManaRegen = NewPassiveRegenEffect(
		AbilitySystemComponent, 
		UGKAttributeSet::GetManaAttribute(), 
		UnitDef.ManaRegen, 
		0.1 
		//, TEXT("GameplayEffect.ManaRegen")
	);
	AddPassiveEffect(ManaRegen);

	auto HealthRegen = NewPassiveRegenEffect(
		AbilitySystemComponent, 
		UGKAttributeSet::GetHealthAttribute(), 
		UnitDef.HealthRegen, 
		0.1
		//, TEXT("GameplayEffect.HealthRegen")
	);
	AddPassiveEffect(HealthRegen);

	// Grant Initial Abilities
	// Need to disable level 0 abilities now
	for (TSubclassOf<UGKGameplayAbility>& AbilityClass : UnitDef.Abilities) {
		FGameplayAbilitySpec Spec(
			AbilityClass,  // TSubclassOf<UGameplayAbility> |
			0,             // int32 InLevel					|
			INDEX_NONE,    // int32 InInputID				| TODO: This is user configured
			nullptr        // UObject * InSourceObject		|
		);

		AbilitySystemComponent->GiveAbility(Spec);
	}
	// ---

	AbilitySystemComponent->Initialized = true;
}

void AGKCharacterBase::K2_GetUnitStatic(FGKUnitStatic& UnitStaticOut, bool& Valid) {
	Valid = false;

	auto Result = GetUnitStatic();
	if (Result != nullptr) {
		UnitStaticOut = *Result;
		Valid = true;
	}
}

FGKUnitStatic* AGKCharacterBase::GetUnitStatic() {
	// We have already a cached lookup
	if (UnitStatic) {
		return UnitStatic;
	}

	if (!UnitDataTable || !UnitRowName.IsValid()) {
		UE_LOG(LogGamekit, Warning, TEXT("Unit not configured to use DataTable"));
		return nullptr;
	}

	UnitStatic = UnitDataTable->FindRow<FGKUnitStatic>(UnitRowName, TEXT("Unit"), true);
		
	if (UnitStatic != nullptr){
		LoadFromDataTable(*UnitStatic);
	}
	else {
		UE_LOG(LogGamekit, Warning, TEXT("Did not find  Unit static"));
	}

	// Listen to data table change
	if (!UnitDataTable->OnDataTableChanged().IsBoundToObject(this)) {
		UnitDataTable->OnDataTableChanged().AddUObject(this, &AGKCharacterBase::OnDataTableChanged_Native);
	}

	return UnitStatic;
}

UAbilitySystemComponent* AGKCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}


void AGKCharacterBase::BeginPlay() {
	Super::BeginPlay();

	// Initialize our abilities
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);

		if (UnitDataTable && UnitRowName.IsValid()) {
			UE_LOG(LogGamekit, Warning, TEXT("Loading Unit Config From DataTable"));
			OnDataTableChanged_Native();
		}
	}
}

void AGKCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RefreshAbilityActorInfo();
	}

	// ASC MixedMode replication requires that the ASC Owner's Owner be the Controller.
	SetOwner(NewController);
}

void AGKCharacterBase::UnPossessed()
{
}

void AGKCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();

	// Our controller changed, must update ActorInfo on AbilitySystemComponent
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RefreshAbilityActorInfo();
	}
}

void AGKCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGKCharacterBase, CharacterLevel);
}

int32 AGKCharacterBase::GetCharacterLevel() const
{
	return CharacterLevel;
}

bool AGKCharacterBase::SetCharacterLevel(int32 NewLevel)
{
	if (CharacterLevel != NewLevel && NewLevel > 0)
	{
		// Our level changed so we need to refresh abilities
		CharacterLevel = NewLevel;
		return true;
	}
	return false;
}

void AGKCharacterBase::GrantAbility(FGKAbilitySlot Slot, TSubclassOf<UGKGameplayAbility> AbilityClass) /* InputID -1 */
{
	// This has input binding that should be customizable
	// 0 is None, The enum for AbilityInput starts at one
	FGameplayAbilitySpec Spec = FGameplayAbilitySpec(
		AbilityClass, 
		1, 
		static_cast<int32>(Slot.SlotNumber), // EGK_MOBA_AbilityInputID::Skill1 + offset
		this
	);
	FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(Spec);
}

UGKGameplayAbility* AGKCharacterBase::GetAbilityInstance(FGKAbilitySlot Slot) {
	auto Spec = AbilitySpecs.Find(Slot);

	if (Spec == nullptr || Spec->Ability == nullptr) {
		return nullptr;
	}

	auto ability = Spec->GetPrimaryInstance();
	if (!ability) {
		ability = Spec->Ability;
	}

	// AbilitySystemComponent->execClientActivateAbilityFailed

	return Cast<UGKGameplayAbility>(ability);
}

FGameplayAbilitySpecHandle AGKCharacterBase::GetAbilityHandle(FGKAbilitySlot Slot) {
	auto Spec = AbilitySpecs.Find(Slot);

	if (Spec == nullptr || Spec->Ability == nullptr) {
		return FGameplayAbilitySpecHandle();
	}

	return Spec->Handle;
}

FGenericTeamId AGKCharacterBase::GetGenericTeamId() const
{
	static const FGenericTeamId PlayerTeam(0);
	static const FGenericTeamId AITeam(1);
	return Cast<APlayerController>(GetController()) ? PlayerTeam : AITeam;
}

void AGKCharacterBase::OnGiveAbility_Native(FGameplayAbilitySpec& AbilitySpec) {
	// AbilityName is the unique tag per ability
	auto Id = FGKAbilitySlot(AbilitySpecs.Num());
	AbilitySpec.InputID = Id.SlotNumber + 1;

	AbilitySpecs.Add(Id, AbilitySpec);
	OnGiveAbility.Broadcast(AbilitySpec);
}

bool AGKCharacterBase::ActivateAbility(FGKAbilitySlot Slot) {
	auto Spec = AbilitySpecs.Find(Slot);

	if (Spec == nullptr || Spec->Ability == nullptr) {
		return false;
	}

	return AbilitySystemComponent->TryActivateAbility(Spec->Handle, true);
}

int AGKCharacterBase::AbilityCount() const {
	return AbilitySpecs.Num();
}

UGKAttributeSet* AGKCharacterBase::GetAttributeSet() {
	return AttributeSet;
}

void AGKCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (!InputsBound && AbilitySystemComponent != nullptr && IsValid(InputComponent)) {

		auto GAIB = FGameplayAbilityInputBinds(
			FString("ConfirmTarget"),
			FString("CancelTarget"),
			FString("EGK_MOBA_AbilityInputID")
			//static_cast<int32>(EGK_MOBA_AbilityInputID::Confirm),
			//static_cast<int32>(EGK_MOBA_AbilityInputID::Cancel)
		);

		AbilitySystemComponent->BindAbilityActivationToInputComponent(
			InputComponent,
			GAIB
		);

		UE_LOG(LogGamekit, Warning, TEXT("Abilities got bound"));
		InputsBound = true;
	}
}