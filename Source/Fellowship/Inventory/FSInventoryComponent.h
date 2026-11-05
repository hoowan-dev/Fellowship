// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/FSInventoryGrid.h"
#include "FSInventoryComponent.generated.h"

class UFSItemDefinition;

USTRUCT()
struct FELLOWSHIP_API FFSInventoryItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UFSItemDefinition> Definition;

	// The top left cell the item covers
	UPROPERTY()
	FIntPoint Cell = FIntPoint::ZeroValue;

	// Clockwise quarter turns, from 0 to 3
	UPROPERTY()
	uint8 Rotation = 0;
};

/**
 * The items a player is carrying. Lives on the player state, so it outlasts the character.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class FELLOWSHIP_API UFSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFSInventoryComponent();

	// Whether there's space for the item, in either orientation
	bool CanAddItem(const UFSItemDefinition* Definition) const;

	// Puts the item in the first open space. Returns false, changing nothing, if it doesn't fit.
	bool AddItem(UFSItemDefinition* Definition);

	FORCEINLINE const FFSInventoryGrid& GetGrid() const { return Grid; }
	FORCEINLINE const TArray<FFSInventoryItem>& GetItems() const { return Items; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "1"))
	FIntPoint GridSize = FIntPoint(12, 8);

	virtual void InitializeComponent() override;

private:
	UPROPERTY(Transient)
	FFSInventoryGrid Grid;

	UPROPERTY(Transient)
	TArray<FFSInventoryItem> Items;

	int32 NextItemId = 0;
};
