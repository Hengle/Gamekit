#include "Widget/GKAbilityWidget.h"

#include "Abilities/Blueprint/GKAsyncTaskAttributeChanged.h"
#include "Abilities/Blueprint/GKAsyncTaskCooldownChanged.h"

#include "Abilities/GKGameplayAbility.h"

void UGKAbilityWidget::SetupListeners(class UGKGameplayAbility* InAbility) {
    if (!InAbility) {
        return;
    }

    Ability = InAbility;
    auto ASC = Ability->GetAbilitySystemComponentFromActorInfo();

    // Attribute
    AttributeChangedTask = UGKAsyncTaskAttributeChanged::ListenForAttributesChange(
        ASC,
        Ability->GetAbilityCostAttribute()
    );

    AttributeChangedTask->OnAttributeChanged.AddDynamic(this, &UGKAbilityWidget::OnAbilityInsufficientResources_Native);

    CooldownChangedTask = UGKAsyncTaskCooldownChanged::ListenForCooldownChange(
        ASC,
        *Ability->GetCooldownTags(),
        true
    );

    CooldownChangedTask->OnCooldownBegin.AddDynamic(this, &UGKAbilityWidget::OnAbilityCooldownBegin_Native);
    CooldownChangedTask->OnCooldownEnd.AddDynamic(this, &UGKAbilityWidget::OnAbilityCooldownEnd_Native);

    // Targeting
    Ability->TargetingStartDelegate.AddDynamic(this, &UGKAbilityWidget::OnStartTargeting);
    Ability->TargetingResultDelegate.AddDynamic(this, &UGKAbilityWidget::OnEndTargeting);

    // Level up
    Ability->OnAbilityLevelUp.AddDynamic(this, &UGKAbilityWidget::OnAbilityLevelUp);

    // Listen to Gameplay effect


    // Start
    // Note Activate should be useless in that case
    CooldownChangedTask->RegisterWithGameInstance(Ability->GetWorld());
    CooldownChangedTask->Activate();

    AttributeChangedTask->RegisterWithGameInstance(Ability->GetWorld());
    AttributeChangedTask->Activate();
}

void UGKAbilityWidget::NativeDestruct() {
    // CooldownChangedTask->EndTask();
    // AttributeChangedTask->EndTask();
}

void UGKAbilityWidget::OnAbilityInsufficientResources_Native(FGameplayAttribute Attribute, float NewValue, float OldValue) {
    return OnAbilityInsufficientResources(Ability->K2_CheckAbilityCost());
}

void UGKAbilityWidget::OnAbilityCooldownBegin_Native(FGameplayTag CooldownTag, float TimeRemaining, float Duration) {
    return OnAbilityCooldownBegin(TimeRemaining, Duration);
}

void UGKAbilityWidget::OnAbilityCooldownEnd_Native(FGameplayTag CooldownTag, float TimeRemaining, float Duration) {
    return OnAbilityCooldownEnd(TimeRemaining, Duration);
}
