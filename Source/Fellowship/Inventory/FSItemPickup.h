// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FSItemPickup.generated.h"

class UFSInventoryComponent;
class UFSItemDefinition;
class UPrimitiveComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FHitResult;

/**
 * An item lying in the world. Works for any item: place one in a level and set its Item.
 */
UCLASS()
class FELLOWSHIP_API AFSItemPickup : public AActor
{
	GENERATED_BODY()

public:
	AFSItemPickup();

	virtual void OnConstruction(const FTransform& Transform) override;

	// Decides this pickup's name, inventory size and mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetItem, Category = "Pickup")
	TObjectPtr<UFSItemDefinition> Item;

	UFUNCTION(BlueprintSetter)
	void SetItem(UFSItemDefinition* NewItem);

	// Moves the item into the inventory and removes this actor. Returns false, changing nothing, if it doesn't fit.
	bool TryPickUp(UFSInventoryComponent* Inventory);

	// What to tell a player standing near this pickup
	FText GetPromptText(const UFSInventoryComponent* Inventory) const;

	// Where the prompt goes: just above the mesh
	FVector GetPromptLocation() const;

protected:
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	// How close a player has to be to pick this up
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> InteractionRange;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (Units = cm))
	float PromptHeight = 30.0f;
};
