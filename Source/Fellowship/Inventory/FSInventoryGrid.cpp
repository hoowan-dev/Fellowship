// Copyright HoowanDev 2026 all rights reserved.

#include "FSInventoryGrid.h"

void FFSInventoryGrid::Init(const FIntPoint& InSize)
{
	Size = InSize;
	Cells.Init(INDEX_NONE, Size.X * Size.Y);
}

FIntPoint FFSInventoryGrid::GetFootprint(const FIntPoint& ItemSize, uint8 Rotation)
{
	return Rotation % 2 == 0 ? ItemSize : FIntPoint(ItemSize.Y, ItemSize.X);
}

bool FFSInventoryGrid::IsInsideGrid(const FIntPoint& Cell, const FIntPoint& Footprint) const
{
	return Footprint.X >= 1 && Footprint.Y >= 1 && Cell.X >= 0 && Cell.Y >= 0 && Cell.X + Footprint.X <= Size.X && Cell.Y + Footprint.Y <= Size.Y;
}

bool FFSInventoryGrid::CanPlace(const FIntPoint& Cell, const FIntPoint& ItemSize, uint8 Rotation) const
{
	const FIntPoint Footprint = GetFootprint(ItemSize, Rotation);

	if (!IsInsideGrid(Cell, Footprint))
	{
		return false;
	}

	for (int32 Y = Cell.Y; Y < Cell.Y + Footprint.Y; ++Y)
	{
		for (int32 X = Cell.X; X < Cell.X + Footprint.X; ++X)
		{
			if (Cells[GetCellIndex(FIntPoint(X, Y))] != INDEX_NONE)
			{
				return false;
			}
		}
	}

	return true;
}

void FFSInventoryGrid::Place(int32 ItemId, const FIntPoint& Cell, const FIntPoint& ItemSize, uint8 Rotation)
{
	check(CanPlace(Cell, ItemSize, Rotation));

	const FIntPoint Footprint = GetFootprint(ItemSize, Rotation);

	for (int32 Y = Cell.Y; Y < Cell.Y + Footprint.Y; ++Y)
	{
		for (int32 X = Cell.X; X < Cell.X + Footprint.X; ++X)
		{
			Cells[GetCellIndex(FIntPoint(X, Y))] = ItemId;
		}
	}
}

void FFSInventoryGrid::Remove(int32 ItemId)
{
	for (int32& CellItemId : Cells)
	{
		if (CellItemId == ItemId)
		{
			CellItemId = INDEX_NONE;
		}
	}
}

bool FFSInventoryGrid::GetItemsCovered(const FIntPoint& Cell, const FIntPoint& ItemSize, uint8 Rotation, TArray<int32>& OutItemIds) const
{
	OutItemIds.Reset();

	const FIntPoint Footprint = GetFootprint(ItemSize, Rotation);

	if (!IsInsideGrid(Cell, Footprint))
	{
		return false;
	}

	for (int32 Y = Cell.Y; Y < Cell.Y + Footprint.Y; ++Y)
	{
		for (int32 X = Cell.X; X < Cell.X + Footprint.X; ++X)
		{
			const int32 ItemId = Cells[GetCellIndex(FIntPoint(X, Y))];
			if (ItemId != INDEX_NONE)
			{
				OutItemIds.AddUnique(ItemId);
			}
		}
	}

	return true;
}

bool FFSInventoryGrid::FindFirstFit(const FIntPoint& ItemSize, FIntPoint& OutCell, uint8& OutRotation) const
{
	for (const uint8 Rotation : { 0, 1 })
	{
		for (int32 Y = 0; Y < Size.Y; ++Y)
		{
			for (int32 X = 0; X < Size.X; ++X)
			{
				if (CanPlace(FIntPoint(X, Y), ItemSize, Rotation))
				{
					OutCell = FIntPoint(X, Y);
					OutRotation = Rotation;
					return true;
				}
			}
		}
	}

	return false;
}

int32 FFSInventoryGrid::GetItemAt(const FIntPoint& Cell) const
{
	const bool bInGrid = Cell.X >= 0 && Cell.X < Size.X && Cell.Y >= 0 && Cell.Y < Size.Y;
	return bInGrid ? Cells[GetCellIndex(Cell)] : INDEX_NONE;
}
