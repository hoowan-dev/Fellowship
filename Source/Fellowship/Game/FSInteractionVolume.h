// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Game/FSInteractable.h"
#include "GameFramework/Actor.h"
#include "FSInteractionVolume.generated.h"

class AFSPlayerController;
class UBoxComponent;

// What an interaction volume does when the player interacts inside it
UENUM(BlueprintType)
enum class EFSInteractionAction : uint8
{
	// Nothing built in, for a Blueprint subclass that overrides Interact
	None,
	CharacterEditor,
	StatsEditor
};

/**
 * An area where pressing interact does something, chosen per placed volume by its Action. Its Display Name floats above it while the player is nearby,
 * with the prompt underneath while they stand inside. Characters keep track of the ones they're standing in. A Blueprint subclass can override Interact for anything Action doesn't cover.
 */
UCLASS()
class FELLOWSHIP_API AFSInteractionVolume : public AActor, public IFSInteractable
{
	GENERATED_BODY()

public:
	AFSInteractionVolume();

	// What interacting inside this volume does
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	EFSInteractionAction Action = EFSInteractionAction::None;

	// Floats above the volume, such as "Character Creator"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText DisplayName;

	// Follows the interact button in the prompt
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText InteractVerb = NSLOCTEXT("FSInteractionVolume", "DefaultInteractVerb", "Interact");

	// What the player's interact button does inside this volume. By default, carries out Action.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AFSPlayerController* PlayerController);

	virtual FFSItemText GetInteractionLabel() const override;

	// Just above the top of the area
	virtual FVector GetInteractionLabelLocation() const override;

	virtual FText GetInteractionVerb(const UFSInventoryComponent* Inventory) const override;

	FORCEINLINE UBoxComponent* GetArea() const { return Area; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

private:
	// Only shown in the editor. Scale or resize it to fit the space.
	UPROPERTY(VisibleAnywhere, Category = "Interaction")
	TObjectPtr<UBoxComponent> Area;

	// How far above the top of the area the label floats
	UPROPERTY(EditAnywhere, Category = "Interaction", meta = (Units = cm))
	float PromptHeight = 20.0f;
};
