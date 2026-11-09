// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/FSInteractable.h"
#include "Inventory/FSInventoryComponent.h"
#include "FSItemPickup.generated.h"

struct FFSItemText;
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
class FELLOWSHIP_API AFSItemPickup : public AActor, public IFSInteractable
{
	GENERATED_BODY()

public:
	AFSItemPickup();

	// Leaves an item lying on the floor under a character's feet, or this far across the floor from them. Returns null if it couldn't be spawned.
	static AFSItemPickup* DropAtFeet(TSubclassOf<AFSItemPickup> PickupClass, UFSItemDefinition* DroppedItem, const ACharacter& Character, const FVector2D& Spread = FVector2D::ZeroVector);

	virtual void OnConstruction(const FTransform& Transform) override;

	// Where a dropped item's mesh sits relative to its pickup
	static FTransform GetDiscardedMeshTransform(const UFSItemDefinition& DroppedItem);

	// How far above the floor a dropped item's pickup sits, so that its mesh rests on the floor
	static double GetFloorClearance(const UFSItemDefinition& DroppedItem);

	// Decides this pickup's name, inventory size and mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetItem, Category = "Pickup")
	TObjectPtr<UFSItemDefinition> Item;

	UFUNCTION(BlueprintSetter)
	void SetItem(UFSItemDefinition* NewItem);

	// Whether this pickup is a backpack, which is worn rather than stored
	bool IsBackpack() const;

	// What a dropped backpack still has inside it, which goes back on with it. Its quick slots' items hang off its mesh.
	void SetBackpackContents(const FFSBackpackContents& Contents);

	// Moves the item into the inventory and removes this actor. Returns false, changing nothing, if it doesn't fit, or if it's a backpack and one is already on.
	bool TryPickUp(UFSInventoryComponent* Inventory);

	// Moves the item onto the inventory's cursor, which needs no room in the grid, and removes this actor. Returns false, changing nothing, if that can't be done.
	bool TryCarry(UFSInventoryComponent* Inventory);

	// Null until something has hung on that side of this backpack
	FORCEINLINE UStaticMeshComponent* GetHungItemMesh(EFSQuickSlot QuickSlot) const { return HungItemMeshes.Get(QuickSlot); }

	// The item's name, in the colour of its rarity
	virtual FFSItemText GetInteractionLabel() const override;

	// Just above the mesh
	virtual FVector GetInteractionLabelLocation() const override;

	// Picking it up, making room for it when it doesn't fit, or swapping it for the backpack being worn
	virtual FText GetInteractionVerb(const UFSInventoryComponent* Inventory) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

	// How close a player has to be to pick this up
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> InteractionRange;

	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// How far above the mesh the label floats
	UPROPERTY(EditAnywhere, Category = "Pickup", meta = (Units = cm))
	float PromptHeight = 30.0f;

private:
	void LogPickedUp() const;

	UPROPERTY(Transient)
	FFSBackpackContents BackpackContents;

	UPROPERTY(Transient)
	FFSHungItemMeshes HungItemMeshes;
};
