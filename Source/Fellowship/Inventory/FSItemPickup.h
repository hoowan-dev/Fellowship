// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inventory/FSInventoryComponent.h"
#include "FSItemPickup.generated.h"

class ACharacter;
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

	// Leaves an item lying on the floor under a character's feet. Returns null if it couldn't be spawned.
	static AFSItemPickup* DropAtFeet(TSubclassOf<AFSItemPickup> PickupClass, UFSItemDefinition* DroppedItem, const ACharacter& Character);

	virtual void OnConstruction(const FTransform& Transform) override;

	// Decides this pickup's name, inventory size and mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetItem, Category = "Pickup")
	TObjectPtr<UFSItemDefinition> Item;

	UFUNCTION(BlueprintSetter)
	void SetItem(UFSItemDefinition* NewItem);

	// Whether this pickup is a backpack, which is worn rather than stored
	bool IsBackpack() const;

	// What a dropped backpack still has inside it, which goes back on with it
	void SetBackpackContents(const FFSBackpackContents& Contents);

	// Moves the item into the inventory and removes this actor. Returns false, changing nothing, if it doesn't fit, or if it's a backpack and one is already on.
	bool TryPickUp(UFSInventoryComponent* Inventory);

	// Moves the item onto the inventory's cursor, which needs no room in the grid, and removes this actor. Returns false, changing nothing, if that can't be done.
	bool TryCarry(UFSInventoryComponent* Inventory);

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

private:
	void LogPickedUp() const;

	UPROPERTY(Transient)
	FFSBackpackContents BackpackContents;
};
