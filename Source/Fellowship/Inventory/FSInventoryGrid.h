// Copyright HoowanDev 2026 all rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FSInventoryGrid.generated.h"

/**
 * Tracks which item covers each cell. Cell (0, 0) is the top left.
 * A rotation is a number of clockwise quarter turns from 0 to 3, where 2 and 3 are the upside down versions of 0 and 1.
 */
USTRUCT()
struct FELLOWSHIP_API FFSInventoryGrid
{
	GENERATED_BODY()

public:
	void Init(const FIntPoint& InSize);

	// The cells an item covers at a rotation: quarter turns swap its width and height
	static FIntPoint GetFootprint(const FIntPoint& ItemSize, uint8 Rotation);

	bool CanPlace(const FIntPoint& Cell, const FIntPoint& ItemSize, uint8 Rotation) const;

	// Marks the cells as covered. The placement has to be one CanPlace allows.
	void Place(int32 ItemId, const FIntPoint& Cell, const FIntPoint& ItemSize, uint8 Rotation);

	// Finds the first open space, going left to right then top to bottom. Items stay upright if they fit anywhere, and otherwise lie on their side.
	bool FindFirstFit(const FIntPoint& ItemSize, FIntPoint& OutCell, uint8& OutRotation) const;

	// The ID of the item covering a cell, or INDEX_NONE if it's empty or outside the grid
	int32 GetItemAt(const FIntPoint& Cell) const;

	FORCEINLINE const FIntPoint& GetSize() const { return Size; }

private:
	FORCEINLINE int32 GetCellIndex(const FIntPoint& Cell) const { return Cell.Y * Size.X + Cell.X; }

	UPROPERTY()
	FIntPoint Size = FIntPoint::ZeroValue;

	UPROPERTY()
	TArray<int32> Cells;
};
